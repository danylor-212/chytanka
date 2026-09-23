#include <gtest/gtest.h>

#include <string>

#include "Utf8.h"

namespace {
bool isValidUtf8(const std::string& s) {
  size_t i = 0;
  while (i < s.size()) {
    const auto lead = static_cast<unsigned char>(s[i]);
    const size_t n = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
    if (n == 0 || i + n > s.size()) return false;
    for (size_t k = 1; k < n; ++k) {
      if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
    }
    i += n;
  }
  return true;
}
}  // namespace

TEST(Utf8AppendBounded, CutsCyrillicOnlyAtCharacterBoundaries) {
  // "Слобода" is 7 two-byte letters; every odd limit falls inside a letter.
  const std::string word = "Слобода";
  for (size_t limit = 0; limit <= word.size() + 1; ++limit) {
    std::string out;
    const bool truncated = utf8AppendBounded(out, word.data(), word.size(), limit);
    EXPECT_TRUE(isValidUtf8(out)) << limit;
    EXPECT_LE(out.size(), limit);
    EXPECT_EQ(truncated, limit < word.size()) << limit;
    EXPECT_EQ(out.size(), limit < word.size() ? limit - limit % 2 : word.size()) << limit;
  }
}

TEST(Utf8AppendBounded, AppendsAcrossChunksLikeExpat) {
  // Expat may hand a title over in several character-data callbacks.
  std::string out;
  EXPECT_FALSE(utf8AppendBounded(out, "Тіні ", 9, 16));
  EXPECT_TRUE(utf8AppendBounded(out, "забутих", 14, 16));
  EXPECT_EQ(out, "Тіні заб");  // 9 + 6 bytes; the next 'у' would need 17
  EXPECT_TRUE(utf8AppendBounded(out, "предків", 14, 16));
  EXPECT_EQ(out, "Тіні заб");
}

TEST(Utf8EllipsizeTruncated, LongCyrillicOpdsTitle) {
  // A long OPDS title cut at the parser's 160-byte title limit.
  const std::string title =
      "Слобожанська хроніка: оповідання, нариси, фейлетони та листи з Харківщини двадцятих років, "
      "упорядковані за першодруками";
  std::string out;
  ASSERT_TRUE(utf8AppendBounded(out, title.data(), title.size(), 160));
  utf8EllipsizeTruncated(out);
  EXPECT_TRUE(isValidUtf8(out));
  // 160 bytes ends inside "років" (mid-letter); cut back to the space before it.
  EXPECT_EQ(out, "Слобожанська хроніка: оповідання, нариси, фейлетони та листи з Харківщини двадцятих…");
}

TEST(Utf8EllipsizeTruncated, KeepsTextWithoutNearbySpace) {
  std::string out = "Надзвичайнодовгеслово";
  utf8EllipsizeTruncated(out);
  EXPECT_EQ(out, "Надзвичайнодовгеслово…");
  std::string trailing = "Кайдашева сім'я, ";
  utf8EllipsizeTruncated(trailing);
  EXPECT_EQ(trailing, "Кайдашева сім'я…");
}
