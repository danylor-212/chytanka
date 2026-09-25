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
