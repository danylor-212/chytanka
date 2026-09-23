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
