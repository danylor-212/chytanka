#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Dictionary.h"
#include "Utf8.h"

namespace fs = std::filesystem;

namespace {

// ---------------------------------------------------------------------------
// StarDict fixture writer. Mirrors scripts/dictionary_tools.py: keys sorted by
// StarDict order (ASCII fold, then bytes), .oft = every 32nd entry offset plus
// a size sentinel, .cspt = 16-byte key prefixes sampled at entries 0 and 16 of
// every .oft page.
// ---------------------------------------------------------------------------

std::string asciiFold(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

bool stardictLess(const std::string& a, const std::string& b) {
  const std::string fa = asciiFold(a);
  const std::string fb = asciiFold(b);
  if (fa != fb) return fa < fb;  // std::string compares as unsigned bytes via char_traits
  return a < b;
}

void appendBe32(std::string& out, const uint32_t v) {
  out += static_cast<char>(v >> 24);
  out += static_cast<char>(v >> 16);
  out += static_cast<char>(v >> 8);
  out += static_cast<char>(v);
}

void appendLe32(std::string& out, const uint32_t v) {
  out += static_cast<char>(v);
  out += static_cast<char>(v >> 8);
  out += static_cast<char>(v >> 16);
  out += static_cast<char>(v >> 24);
}

// Byte offsets of every entry in a .idx (suffix 8) or .syn (suffix 4) blob.
std::vector<uint32_t> entryOffsets(const std::string& data, const size_t suffix) {
  std::vector<uint32_t> offsets;
  size_t pos = 0;
  while (pos < data.size()) {
    offsets.push_back(static_cast<uint32_t>(pos));
    pos = data.find('\0', pos) + 1 + suffix;
  }
  return offsets;
}

std::string buildOft(const std::string& data, const size_t suffix) {
  std::string out("StarDict's Cache, Version: 0.2");
  out += std::string("\xc1\xd1\xa4\x51\x00\x00\x00\x00", 8);
  const auto offsets = entryOffsets(data, suffix);
  for (size_t i = 32; i < offsets.size(); i += 32) appendLe32(out, offsets[i]);
  appendLe32(out, static_cast<uint32_t>(data.size()));
  return out;
}

std::string buildCspt(const std::string& data, const size_t suffix) {
  const auto offsets = entryOffsets(data, suffix);
  std::string entries;
  uint32_t count = 0;
  for (size_t i = 0; i < offsets.size(); i += 16) {
    const uint32_t pos = offsets[i];
    std::string prefix = data.substr(pos, data.find('\0', pos) - pos).substr(0, 16);
    prefix.resize(16, '\0');
    entries += prefix;
    appendLe32(entries, pos);
    count++;
  }
  std::string out("CSPT");
  out += '\x01';
  out += '\x10';
  out += '\x10';
  out += '\x00';
  appendLe32(out, count);
  return out + entries;
}

void writeFile(const fs::path& path, const std::string& bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

struct DictFixture {
  // headword -> definition; form -> headword
  std::vector<std::pair<std::string, std::string>> words;
  std::vector<std::pair<std::string, std::string>> forms;
  bool oft = true;
  bool cspt = true;
  bool synIndex = true;
};

// Writes the fixture and returns the StarDict base path ("<dir>/test").
std::string writeDictionary(const fs::path& dir, DictFixture fixture) {
  fs::remove_all(dir);
  fs::create_directories(dir);
  std::sort(fixture.words.begin(), fixture.words.end(),
            [](const auto& a, const auto& b) { return stardictLess(a.first, b.first); });
  std::map<std::string, uint32_t> ordinal;
  std::string idx;
  std::string dict;
  for (const auto& [word, definition] : fixture.words) {
    ordinal[word] = static_cast<uint32_t>(ordinal.size());
    idx += word;
    idx += '\0';
    appendBe32(idx, static_cast<uint32_t>(dict.size()));
    appendBe32(idx, static_cast<uint32_t>(definition.size()));
    dict += definition;
  }
  const fs::path base = dir / "test";
  writeFile(base.string() + ".ifo", "StarDict's dict ifo file\nversion=3.0.0\nbookname=Test\nwordcount=" +
                                        std::to_string(fixture.words.size()) + "\nsametypesequence=m\n");
  writeFile(base.string() + ".idx", idx);
  writeFile(base.string() + ".dict", dict);
  if (fixture.oft) writeFile(base.string() + ".idx.oft", buildOft(idx, 8));
  if (fixture.cspt) writeFile(base.string() + ".idx.oft.cspt", buildCspt(idx, 8));

  if (!fixture.forms.empty()) {
    std::sort(fixture.forms.begin(), fixture.forms.end(),
              [](const auto& a, const auto& b) { return stardictLess(a.first, b.first); });
    std::string syn;
    for (const auto& [form, headword] : fixture.forms) {
      syn += form;
      syn += '\0';
      appendBe32(syn, ordinal.at(headword));
    }
    writeFile(base.string() + ".syn", syn);
    if (fixture.synIndex) {
      writeFile(base.string() + ".syn.oft", buildOft(syn, 4));
      if (fixture.cspt) writeFile(base.string() + ".syn.oft.cspt", buildCspt(syn, 4));
    }
  }
  return base.string();
}

DictFixture wordsOnly(std::vector<std::pair<std::string, std::string>> words) {
  DictFixture fixture;
  fixture.words = std::move(words);
  return fixture;
}

class DictionaryLookupTest : public ::testing::Test {
 protected:
  void TearDown() override {
    Dictionary::clearLookupDictPathOverride();
    fs::remove_all(root);
  }

  std::string install(const DictFixture& fixture) {
    const std::string base = writeDictionary(root, fixture);
    Dictionary::setLookupDictPathOverride(base.c_str());
    return base;
  }

  std::string lookupHeadword(const std::string& word) {
    bool stem = false;
    const DictLocation loc = Dictionary::locateWithStemVariants(word, &stem);
    EXPECT_FALSE(loc.readError) << word;
    return loc.found ? loc.headword : std::string();
  }

  fs::path root =
      fs::temp_directory_path() /
      ("crossink-dict-test-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
};

// 3 * 3 * 3 * 3 = 81 distinct Cyrillic endings, all sharing an 8-letter
// (16-byte) prefix so every .cspt sample in that range is truncated.
std::vector<std::string> sharedPrefixWords(const std::string& prefix) {
  const char* letters[] = {"а", "й", "м"};
  std::vector<std::string> out;
  for (const char* a : letters)
    for (const char* b : letters)
      for (const char* c : letters)
        for (const char* d : letters) out.push_back(prefix + a + b + c + d);
  // Shorter members of the same family, like real inflection tables.
  out.push_back(prefix + "й");
  out.push_back(prefix + "м");
  out.push_back(prefix + "ми");
  out.push_back(prefix + "х");
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Case folding helpers (shared with hyphenation)
// ---------------------------------------------------------------------------

TEST(Utf8CaseFold, LowercasesUkrainianAndLatin) {
  EXPECT_EQ(utf8ToLower("ҐЄІЇ ЙЦУКЕН ЩЬЮЯ"), "ґєії йцукен щьюя");
  EXPECT_EQ(utf8ToLower("Київ"), "київ");
  EXPECT_EQ(utf8ToLower("Hello ÀÉÎ ŁÓDŹ"), "hello àéî łódź");
  EXPECT_EQ(utf8ToLower("вже малі"), "вже малі");
}

TEST(Utf8CaseFold, KeepsMalformedBytes) {
  const std::string malformed = std::string("A\xFF") + "Б";
  EXPECT_EQ(utf8ToLower(malformed), std::string("a\xFF") + "б");
}

// ---------------------------------------------------------------------------
// Normalisation contract
// ---------------------------------------------------------------------------

TEST(DictionaryKeyNormalisation, FoldsApostrophesToRightSingleQuote) {
  const std::string canonical = "м’ясо";
  for (const char* input : {"м'ясо", "м`ясо", "м´ясо", "мʼясо", "м‘ясо", "м’ясо"}) {
    EXPECT_EQ(Dictionary::normalizeLookupKey(input), canonical) << input;
  }
}

TEST(DictionaryKeyNormalisation, StripsCombiningAcuteStress) {
  EXPECT_EQ(Dictionary::normalizeLookupKey("кни\xCC\x81жка"), "книжка");
  EXPECT_EQ(Dictionary::normalizeLookupKey("ї\xCC\x81жак"), "їжак");
  // Only U+0301 is removed; precomposed Latin letters stay intact.
  EXPECT_EQ(Dictionary::normalizeLookupKey("café"), "café");
  EXPECT_EQ(Dictionary::normalizeLookupKey("plain"), "plain");
}

TEST(DictionaryKeyNormalisation, VariantOrder) {
  using V = std::vector<std::string>;
  EXPECT_EQ(Dictionary::lookupKeyVariants("книжка"), (V{"книжка"}));
  EXPECT_EQ(Dictionary::lookupKeyVariants("Книжка"), (V{"Книжка", "книжка"}));
  EXPECT_EQ(Dictionary::lookupKeyVariants("КИЇВ"), (V{"КИЇВ", "київ", "Київ"}));
  // ASCII case is already folded by the index comparison.
  EXPECT_EQ(Dictionary::lookupKeyVariants("Hello"), (V{"Hello"}));
  EXPECT_EQ(Dictionary::lookupKeyVariants("Мʼясо"), (V{"Мʼясо", "М’ясо", "м’ясо", "м'ясо"}));
  EXPECT_EQ(Dictionary::lookupKeyVariants("don’t"), (V{"don’t", "don't"}));
  EXPECT_TRUE(Dictionary::lookupKeyVariants("").empty());
}

// ---------------------------------------------------------------------------
// Lookup through the index
// ---------------------------------------------------------------------------

TEST_F(DictionaryLookupTest, CyrillicCaseFoldingFindsLowercaseHeadwords) {
  install(wordsOnly({{"книжка", "book"},
                     {"ґанок", "porch"},
                     {"їжак", "hedgehog"},
                     {"європа", "europe"},
                     {"ірис", "iris"},
                     {"Київ", "Kyiv"},
                     {"hello", "greeting"}}));
  EXPECT_EQ(lookupHeadword("книжка"), "книжка");
  EXPECT_EQ(lookupHeadword("Книжка"), "книжка");
  EXPECT_EQ(lookupHeadword("КНИЖКА"), "книжка");
  EXPECT_EQ(lookupHeadword("Ґанок"), "ґанок");
  EXPECT_EQ(lookupHeadword("ЇЖАК"), "їжак");
  EXPECT_EQ(lookupHeadword("Європа"), "європа");
  EXPECT_EQ(lookupHeadword("Ірис"), "ірис");
  EXPECT_EQ(lookupHeadword("Київ"), "Київ");
  EXPECT_EQ(lookupHeadword("КИЇВ"), "Київ");
  EXPECT_EQ(lookupHeadword("HELLO"), "hello");
  EXPECT_EQ(lookupHeadword("кніжка"), "");
}

TEST_F(DictionaryLookupTest, ExactCaseStillPreferred) {
  install(wordsOnly({{"Polish", "from Poland"}, {"polish", "to shine"}, {"Тесла", "name"}, {"тесла", "adze"}}));
  EXPECT_EQ(lookupHeadword("Polish"), "Polish");
  EXPECT_EQ(lookupHeadword("polish"), "polish");
  EXPECT_EQ(lookupHeadword("Тесла"), "Тесла");
  EXPECT_EQ(lookupHeadword("тесла"), "тесла");
  EXPECT_EQ(lookupHeadword("ТЕСЛА"), "тесла");
}

TEST_F(DictionaryLookupTest, ApostropheAndStressNormalisation) {
  install(wordsOnly({{"м’ясо", "meat"}, {"книжка", "book"}, {"don't", "do not"}}));
  EXPECT_EQ(lookupHeadword("м'ясо"), "м’ясо");
  EXPECT_EQ(lookupHeadword("мʼясо"), "м’ясо");
  EXPECT_EQ(lookupHeadword("М‘ЯСО"), "м’ясо");
  EXPECT_EQ(lookupHeadword("кни\xCC\x81жка"), "книжка");
  EXPECT_EQ(lookupHeadword("Кни\xCC\x81жка"), "книжка");
  // Dictionaries that key on the ASCII apostrophe still match typographic input.
  EXPECT_EQ(lookupHeadword("don’t"), "don't");
}

TEST_F(DictionaryLookupTest, EnglishStemmingUnchanged) {
  install(wordsOnly({{"book", "a text"}, {"run", "move"}}));
  bool stem = false;
  const DictLocation loc = Dictionary::locateWithStemVariants("Books", &stem);
  EXPECT_TRUE(loc.found);
  EXPECT_TRUE(stem);
  EXPECT_EQ(loc.headword, "book");
}

// Regression: .cspt samples hold only the first 16 bytes of a key. Before the
// fix, a sample "книжкови|ми" truncated to "книжкови" compared below the
// target "книжковий", the search started at that sample, and the scan stopped
// at once because the full sampled word sorts after the target.
TEST_F(DictionaryLookupTest, CsptTruncatedPrefixesDoNotHideLongCyrillicWords) {
  DictFixture fixture;
  std::vector<std::string> all;
  for (const std::string prefix : {"книжкови", "книжково", "перепродаж"}) {
    const auto family = sharedPrefixWords(prefix);
    all.insert(all.end(), family.begin(), family.end());
  }
  for (const char* filler : {"а", "б", "в", "книга", "книжка", "кнур", "пере", "я"}) all.emplace_back(filler);
  for (const auto& word : all) fixture.words.emplace_back(word, "def:" + word);
  install(fixture);

  for (const auto& word : all) {
    const DictLocation loc = Dictionary::locate(word);
    EXPECT_TRUE(loc.found) << word;
    EXPECT_EQ(loc.headword, word);
    EXPECT_EQ(Dictionary::lookup(word), "def:" + word);
  }
  EXPECT_FALSE(Dictionary::locate("книжковиї").found);
  EXPECT_FALSE(Dictionary::locate("книжкови").found);
}

TEST_F(DictionaryLookupTest, CsptTruncatedPrefixesInSynFile) {
  DictFixture fixture;
  fixture.words = {{"книжковий", "bookish"}, {"перепродати", "resell"}};
  std::vector<std::string> forms;
  for (const auto& form : sharedPrefixWords("книжкови")) {
    if (form != "книжковий") forms.push_back(form);
  }
  for (const auto& form : sharedPrefixWords("перепродаж")) forms.push_back(form);
  for (const auto& form : forms) {
    fixture.forms.emplace_back(form, form.rfind("книжков", 0) == 0 ? "книжковий" : "перепродати");
  }
  install(fixture);

  for (const auto& form : forms) {
    const DictLocation loc = Dictionary::locateAltForm(form);
    EXPECT_TRUE(loc.found) << form;
    EXPECT_EQ(loc.headword, form.rfind("книжков", 0) == 0 ? "книжковий" : "перепродати") << form;
  }
}

// ---------------------------------------------------------------------------
// Inflected forms through .syn
// ---------------------------------------------------------------------------

namespace {
DictFixture inflectionFixture() {
  DictFixture fixture;
  // Enough headwords that .syn ordinals cross several .oft pages.
  for (int i = 0; i < 100; i++) {
    char word[32];
    snprintf(word, sizeof(word), "слово%03d", i);
    fixture.words.emplace_back(word, std::string("def:") + word);
  }
  fixture.words.emplace_back("книжка", "def:книжка");
  fixture.words.emplace_back("м’ясо", "def:м’ясо");
  fixture.words.emplace_back("ялинка", "def:ялинка");
  for (const char* form : {"книжки", "книжці", "книжку", "книжкою", "книжок", "книжкам", "книжками", "книжках"}) {
    fixture.forms.emplace_back(form, "книжка");
  }
  fixture.forms.emplace_back("м’яса", "м’ясо");
  fixture.forms.emplace_back("ялинками", "ялинка");
  for (int i = 0; i < 100; i++) {
    char form[32];
    char lemma[32];
    snprintf(form, sizeof(form), "слова%03d", i);
    snprintf(lemma, sizeof(lemma), "слово%03d", i);
    fixture.forms.emplace_back(form, lemma);
  }
  return fixture;
}
}  // namespace

TEST_F(DictionaryLookupTest, AltFormResolvesToLemmaLocation) {
  install(inflectionFixture());
  EXPECT_TRUE(Dictionary::hasIndexedAltForms());

  const DictLocation loc = Dictionary::locateAltForm("книжками");
  ASSERT_TRUE(loc.found);
  EXPECT_EQ(loc.headword, "книжка");
  EXPECT_EQ(Dictionary::lookup(loc.headword), "def:книжка");
  // The .syn result must point at the same .dict range as a direct lookup.
  const DictLocation direct = Dictionary::locate("книжка");
  EXPECT_EQ(loc.offset, direct.offset);
  EXPECT_EQ(loc.size, direct.size);

  EXPECT_EQ(Dictionary::locateAltForm("Книжками").headword, "книжка");
  EXPECT_EQ(Dictionary::locateAltForm("КНИЖКАМИ").headword, "книжка");
  EXPECT_EQ(Dictionary::locateAltForm("кни\xCC\x81жками").headword, "книжка");
  EXPECT_EQ(Dictionary::locateAltForm("Мʼяса").headword, "м’ясо");
  EXPECT_EQ(Dictionary::resolveAltForm("книжці"), "книжка");
  for (int i = 0; i < 100; i += 7) {
    char form[32];
    char lemma[32];
    snprintf(form, sizeof(form), "слова%03d", i);
    snprintf(lemma, sizeof(lemma), "слово%03d", i);
    const DictLocation byForm = Dictionary::locateAltForm(form);
    ASSERT_TRUE(byForm.found) << form;
    EXPECT_EQ(byForm.headword, lemma);
    EXPECT_EQ(Dictionary::lookup(byForm.headword), std::string("def:") + lemma);
  }
  EXPECT_FALSE(Dictionary::locateAltForm("книжкар").found);
}

TEST_F(DictionaryLookupTest, AltFormsWithoutIndexNeedPrompt) {
  DictFixture fixture = inflectionFixture();
  fixture.synIndex = false;
  install(fixture);
  EXPECT_TRUE(Dictionary::hasAltForms());
  EXPECT_FALSE(Dictionary::hasIndexedAltForms());
  // Still resolvable when the user confirms the prompt (full .syn scan).
  EXPECT_EQ(Dictionary::locateAltForm("книжками").headword, "книжка");
}

TEST_F(DictionaryLookupTest, WorksWithOftOnlyAndWithoutAccelerators) {
  DictFixture fixture = inflectionFixture();
  fixture.cspt = false;
  install(fixture);
  EXPECT_EQ(lookupHeadword("Ялинка"), "ялинка");
  EXPECT_EQ(Dictionary::locateAltForm("Ялинками").headword, "ялинка");

  fixture.oft = false;
  fixture.synIndex = false;
  install(fixture);
  EXPECT_EQ(lookupHeadword("Ялинка"), "ялинка");
  EXPECT_EQ(Dictionary::locateAltForm("Ялинками").headword, "ялинка");
}

TEST_F(DictionaryLookupTest, SuggestionsUseFoldedKey) {
  install(wordsOnly({{"книжка", "book"}, {"книжник", "bookman"}, {"кит", "whale"}, {"Київ", "Kyiv"}}));
  const auto similar = Dictionary::findSimilar("Кнжка", 3);
  ASSERT_FALSE(similar.empty());
  EXPECT_EQ(similar.front(), "книжка");
}
