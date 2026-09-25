// Lookup cost on a prepared StarDict dictionary (host only, not a ctest).
//
//   DictionaryLookupBench <folder>/<stem> [sampleEvery]
//
// Resolves every sampleEvery-th .idx headword directly and every
// sampleEvery-th .syn form through locateAltForm(), then prints misses and the
// average SD work per lookup (file opens, seeks, 512-byte sector loads through
// one shared cache, bytes read) plus host time. Run it with and without the
// .cspt files to compare accelerators.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Dictionary.h"
#include "HalStorage.h"

namespace {

std::vector<std::string> readKeys(const std::string& path, const size_t suffix, const size_t every) {
  std::ifstream in(path, std::ios::binary);
  const std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::vector<std::string> keys;
  size_t pos = 0;
  size_t n = 0;
  while (pos < data.size()) {
    const size_t end = data.find('\0', pos);
    if (end == std::string::npos) break;
    if (n++ % every == 0) keys.emplace_back(data, pos, end - pos);
    pos = end + 1 + suffix;
  }
  return keys;
}

struct Totals {
  size_t lookups = 0;
  size_t misses = 0;
  HalStorageStats io;
  double seconds = 0;
};

template <typename Fn>
Totals run(const std::vector<std::string>& keys, Fn&& fn) {
  Totals t;
  halStorageStats.reset();
  const auto start = std::chrono::steady_clock::now();
  for (const auto& key : keys) {
    t.lookups++;
    if (!fn(key)) {
      if (t.misses < 5) std::printf("    miss: %s\n", key.c_str());
      t.misses++;
    }
  }
  t.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  t.io = halStorageStats;
  return t;
}

void report(const char* label, const Totals& t) {
  const double n = t.lookups ? static_cast<double>(t.lookups) : 1.0;
  std::printf("%-28s n=%-7zu miss=%-6zu opens=%.1f seeks=%.1f sectors=%.1f bytes=%.0f host=%.1fus\n", label, t.lookups,
              t.misses, t.io.opens / n, t.io.seeks / n, t.io.sectorLoads / n, t.io.bytesRead / n, t.seconds * 1e6 / n);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <folder>/<stem> [sampleEvery]\n", argv[0]);
    return 2;
  }
  const std::string base = argv[1];
  const size_t every = argc > 2 ? std::strtoul(argv[2], nullptr, 10) : 1;
  Dictionary::setLookupDictPathOverride(base.c_str());

  const auto heads = readKeys(base + ".idx", 8, every);
  const auto forms = readKeys(base + ".syn", 4, every);
  std::printf("%zu headwords, %zu forms sampled (every %zu)\n", heads.size(), forms.size(), every);

  report("idx direct (locate)", run(heads, [](const std::string& w) { return Dictionary::locate(w).found; }));
  report("syn form (locateAltForm)",
         run(forms, [](const std::string& w) { return Dictionary::locateAltForm(w).found; }));

  // Full controller path for an inflected word: direct probes miss, then .syn.
  bool stem = false;
  report("inflected, full path", run(forms, [&stem](const std::string& w) {
           const auto direct = Dictionary::locateWithStemVariants(w, &stem);
           if (direct.found) return true;
           return Dictionary::locateAltForm(w).found;
         }));
  return 0;
}
