#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "lib/Epub/Epub/hyphenation/HyphenationCommon.h"
#include "lib/Epub/Epub/hyphenation/Hyphenator.h"

namespace {

std::vector<size_t> offsets(const std::string& word) {
  std::vector<size_t> out;
  for (const auto& b : Hyphenator::breakOffsets(word, /*includeFallback=*/false)) out.push_back(b.byteOffset);
  return out;
}

const Hyphenator::BreakInfo* breakAt(const std::vector<Hyphenator::BreakInfo>& breaks, size_t byteOffset) {
  for (const auto& b : breaks) {
    if (b.byteOffset == byteOffset) return &b;
  }
  return nullptr;
}

}  // namespace

TEST(ToLowerCyrillic, BasicRangeUnchanged) {
  EXPECT_EQ(toLowerCyrillic(0x0410), 0x0430u);  // А -> а
  EXPECT_EQ(toLowerCyrillic(0x042F), 0x044Fu);  // Я -> я
  EXPECT_EQ(toLowerCyrillic(0x0401), 0x0451u);  // Ё -> ё
  EXPECT_EQ(toLowerCyrillic(0x0430), 0x0430u);  // а stays
}

TEST(ToLowerCyrillic, UkrainianCapitals) {
  EXPECT_EQ(toLowerCyrillic(0x0404), 0x0454u);  // Є -> є
  EXPECT_EQ(toLowerCyrillic(0x0406), 0x0456u);  // І -> і
  EXPECT_EQ(toLowerCyrillic(0x0407), 0x0457u);  // Ї -> ї
  EXPECT_EQ(toLowerCyrillic(0x0490), 0x0491u);  // Ґ -> ґ
}

TEST(ToLowerCyrillic, OtherCyrillicCapitals) {
  EXPECT_EQ(toLowerCyrillic(0x040E), 0x045Eu);  // Ў -> ў (Belarusian)
  EXPECT_EQ(toLowerCyrillic(0x0408), 0x0458u);  // Ј -> ј (Serbian)
  EXPECT_EQ(toLowerCyrillic(0x0462), 0x0463u);  // Ѣ -> ѣ
  EXPECT_EQ(toLowerCyrillic(0x04C1), 0x04C2u);  // Ӂ -> ӂ (odd-upper block)
  EXPECT_EQ(toLowerCyrillic(0x04C0), 0x04CFu);  // Ӏ -> ӏ (palochka)
  EXPECT_EQ(toLowerCyrillic(0x04D8), 0x04D9u);  // Ә -> ә
  EXPECT_EQ(toLowerCyrillic(0x0500), 0x0501u);  // Ԁ -> ԁ
}

TEST(ToLowerCyrillic, NonLettersAndLowercaseUnchanged) {
  EXPECT_EQ(toLowerCyrillic(0x0491), 0x0491u);  // ґ stays
  EXPECT_EQ(toLowerCyrillic(0x0482), 0x0482u);  // ҂ thousands sign
  EXPECT_EQ(toLowerCyrillic(0x0483), 0x0483u);  // combining titlo
  EXPECT_EQ(toLowerCyrillic(0x04C2), 0x04C2u);  // ӂ stays
  EXPECT_EQ(toLowerCyrillic('A'), static_cast<uint32_t>('A'));
}

// A capitalised word must hyphenate exactly like its lowercase form. All pairs below have
// equal UTF-8 byte lengths, so byte offsets are directly comparable.
TEST(UkrainianHyphenation, CapitalisedWordsMatchLowercase) {
  Hyphenator::setPreferredLanguage("uk");
  const std::vector<std::pair<std::string, std::string>> pairs = {
      {"Європейський", "європейський"},
      {"Іваненківський", "іваненківський"},
      {"Їжакуватий", "їжакуватий"},
      {"Ґалаґанівський", "ґалаґанівський"},
      {"Український", "український"},  // control: basic А..Я range
      // Corpus words, gold Єд=на=ла / єд=на=ти (offsets 4,8).
      {"Єднала", "єднала"},
      {"Єднати", "єднати"},
      // ALL-CAPS heading forms: capital І mid-word must fold too.
      {"НІКОЛИ", "ніколи"},
      {"ЧОЛОВІКА", "чоловіка"},
  };
  for (const auto& [upper, lower] : pairs) {
    const auto lowerOffsets = offsets(lower);
    ASSERT_FALSE(lowerOffsets.empty()) << "no breaks at all for " << lower;
    EXPECT_EQ(offsets(upper), lowerOffsets) << upper;
  }
}

// U+02BC MODIFIER LETTER APOSTROPHE is the standard Ukrainian apostrophe. It must hyphenate
// exactly like the ASCII apostrophe form, shifted by the 1 extra byte U+02BC (2 bytes) costs
// over ASCII "'" (1 byte) at every offset from the apostrophe onward.
TEST(UkrainianHyphenation, ModifierApostropheWordIsHyphenatable) {
  Hyphenator::setPreferredLanguage("uk");
  const auto modifierOffsets = offsets("обʼєднання");
  const auto asciiOffsets = offsets("об'єднання");
  ASSERT_FALSE(modifierOffsets.empty());
  ASSERT_EQ(modifierOffsets.size(), asciiOffsets.size());
  constexpr size_t kApostropheByteOffset = 4;  // "об" = 4 bytes.
  for (size_t i = 0; i < modifierOffsets.size(); ++i) {
    const size_t expected = asciiOffsets[i] >= kApostropheByteOffset ? asciiOffsets[i] + 1 : asciiOffsets[i];
    EXPECT_EQ(modifierOffsets[i], expected);
  }
}

// "під" = 6 bytes, "ʼ" (U+02BC) = 2 bytes -> "ї" starts at byte 8.
TEST(ApostropheBreaks, ModifierApostropheInCyrillicInsertsHyphen) {
  Hyphenator::setPreferredLanguage("uk");
  const auto breaks = Hyphenator::breakOffsets("підʼїзд", false);
  const auto* b = breakAt(breaks, 8);
  ASSERT_NE(b, nullptr) << "expected a break right after the apostrophe";
  EXPECT_TRUE(b->requiresInsertedHyphen) << "Ukrainian apostrophe break must show a hyphen";
}

// ASCII apostrophe between Cyrillic letters: "під" = 6 bytes, "'" = 1 byte -> "ї" at byte 7.
TEST(ApostropheBreaks, AsciiApostropheInCyrillicInsertsHyphen) {
  Hyphenator::setPreferredLanguage("uk");
  const auto breaks = Hyphenator::breakOffsets("під'їзд", false);
  const auto* b = breakAt(breaks, 7);
  ASSERT_NE(b, nullptr);
  EXPECT_TRUE(b->requiresInsertedHyphen);
}

// U+02BC is a letter (a modifier, not elision punctuation) in any script, so it inserts a
// hyphen even between Latin letters. "abc" = 3 bytes, "ʼ" (U+02BC) = 2 bytes -> "defg" at byte 5.
TEST(ApostropheBreaks, ModifierApostropheInLatinInsertsHyphen) {
  Hyphenator::setPreferredLanguage("en");
  const auto breaks = Hyphenator::breakOffsets("abcʼdefg", false);
  const auto* b = breakAt(breaks, 5);
  ASSERT_NE(b, nullptr);
  EXPECT_TRUE(b->requiresInsertedHyphen);
}

// Latin elision keeps breaking without an inserted hyphen.
TEST(ApostropheBreaks, LatinElisionBreaksWithoutHyphen) {
  Hyphenator::setPreferredLanguage("it");
  const auto breaks = Hyphenator::breakOffsets("all'improvviso", false);
  const auto* b = breakAt(breaks, 4);
  ASSERT_NE(b, nullptr);
  EXPECT_FALSE(b->requiresInsertedHyphen);
}
