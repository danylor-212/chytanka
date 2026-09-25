#include "Utf8.h"

#include <cstring>

#include "Utf8ComposeTable.h"

namespace {
// Look up canonical composition, including algorithmic Hangul LV / LVT pairs.
uint32_t utf8ComposePair(const uint32_t base, const uint32_t mark) {
  if (base >= 0x1100 && base <= 0x1112 && mark >= 0x1161 && mark <= 0x1175) {
    return 0xAC00 + (base - 0x1100) * 588 + (mark - 0x1161) * 28;
  }
  if (base >= 0xAC00 && base <= 0xD7A3 && (base - 0xAC00) % 28 == 0 && mark >= 0x11A8 && mark <= 0x11C2) {
    return base + mark - 0x11A7;
  }
  if (!utf8IsCombiningMark(mark) || base > 0xFFFF || mark > 0xFFFF) return 0;
  int lo = 0;
  int hi = kUtf8ComposeTableSize - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const Utf8ComposeEntry& e = kUtf8ComposeTable[mid];
    if (e.base < base || (e.base == base && e.mark < mark)) {
      lo = mid + 1;
    } else if (e.base > base || (e.base == base && e.mark > mark)) {
      hi = mid - 1;
    } else {
      return e.composed;
    }
  }
  return 0;
}

bool isLookupBoundary(const uint32_t cp) {
  if (cp <= 0x7F) {
    return !((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'));
  }

  // Controls and Unicode whitespace.
  if ((cp >= 0x80 && cp <= 0xA0) || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) || (cp >= 0x2028 && cp <= 0x202F) ||
      cp == 0x205F || cp == 0x3000) {
    return true;
  }

  // Latin-1 punctuation and symbols sit immediately before accented Latin.
  if (cp >= 0x00A1 && cp <= 0x00BF) return true;

  // Common punctuation and symbol blocks. Keeping this as range checks avoids
  // pulling a full Unicode-category library into constrained C3 firmware.
  if ((cp >= 0x2000 && cp <= 0x2BFF) || (cp >= 0x2E00 && cp <= 0x2E7F) || (cp >= 0x3000 && cp <= 0x303F) ||
      (cp >= 0xFE10 && cp <= 0xFE6F) || (cp >= 0x1F000 && cp <= 0x1FAFF)) {
    return true;
  }

  // Fullwidth punctuation/symbols, excluding fullwidth letters and digits.
  if ((cp >= 0xFF01 && cp <= 0xFF0F) || (cp >= 0xFF1A && cp <= 0xFF20) || (cp >= 0xFF3B && cp <= 0xFF40) ||
      (cp >= 0xFF5B && cp <= 0xFF65) || (cp >= 0xFFE0 && cp <= 0xFFEE)) {
    return true;
  }

  // Script-specific punctuation that lives beside letters rather than in a
  // dedicated punctuation block.
  return cp == 0x037E || cp == 0x0387 || (cp >= 0x055A && cp <= 0x055F) || cp == 0x0589 || cp == 0x058A ||
         cp == 0x05BE || cp == 0x05C0 || cp == 0x05C3 || cp == 0x05C6 || (cp >= 0x0609 && cp <= 0x060D) ||
         cp == 0x061B || (cp >= 0x061D && cp <= 0x061F) || (cp >= 0x066A && cp <= 0x066D) || cp == 0x06D4 ||
         (cp >= 0x0700 && cp <= 0x070D) || cp == 0x0964 || cp == 0x0965;
}

bool isLookupCoreCharacter(const uint32_t cp) {
  return cp != 0 && cp != REPLACEMENT_GLYPH && !utf8IsCombiningMark(cp) && !isLookupBoundary(cp);
}
}  // namespace

std::string utf8ComposeNfc(const std::string& in) {
  // Fast path: NFC composition can only change text that contains a combining
  // diacritical mark U+0300-036F (UTF-8 lead byte 0xCC or 0xCD) or conjoining
  // Hangul jamo (lead byte 0xE1 for U+1000-1FFF). Plain ASCII and
  // already-precomposed (NFC) text -- the vast majority of words -- have none, so
  // return them untouched without walking codepoints or allocating. A 0xCD or
  // 0xE1 that is actually a non-composing codepoint (e.g. Georgian, Cherokee)
  // just falls through to the full pass below.
  bool maybeHasMarks = false;
  for (const unsigned char c : in) {
    if (c == 0xCC || c == 0xCD || c == 0xE1) {
      maybeHasMarks = true;
      break;
    }
  }
  if (!maybeHasMarks) return in;

  std::string out;
  out.reserve(in.size());
  const unsigned char* p = reinterpret_cast<const unsigned char*>(in.c_str());
  uint32_t base = 0;
  bool haveBase = false;
  while (*p) {
    const uint32_t cp = utf8NextCodepoint(&p);
    if (cp == 0) break;
    const uint32_t composed = haveBase ? utf8ComposePair(base, cp) : 0;
    if (composed) {
      base = composed;  // keep accumulating marks or trailing jamo
      continue;
    }
    if (utf8IsCombiningMark(cp)) {
      // No composition: flush the pending base, then emit the mark unchanged.
      if (haveBase) {
        utf8AppendCodepoint(base, out);
        haveBase = false;
      }
      utf8AppendCodepoint(cp, out);
    } else {
      if (haveBase) utf8AppendCodepoint(base, out);
      base = cp;
      haveBase = true;
    }
  }
  if (haveBase) utf8AppendCodepoint(base, out);
  return out;
}

void utf8ComposeNfcInPlace(char* buffer) {
  const auto* read = reinterpret_cast<const unsigned char*>(buffer);
  char* write = buffer;
  char* baseStart = buffer;
  uint32_t base = 0;
  while (*read) {
    const auto* start = read;
    const uint32_t cp = utf8NextCodepoint(&read);
    const uint32_t composed = utf8ComposePair(base, cp);
    if (composed) {
      // All supported compositions are BMP codepoints and fit within the
      // consumed pair's bytes, so rewriting the base cannot overtake read.
      write = baseStart;
      if (composed < 0x800) {
        *write++ = static_cast<char>(0xC0 | (composed >> 6));
      } else {
        *write++ = static_cast<char>(0xE0 | (composed >> 12));
        *write++ = static_cast<char>(0x80 | ((composed >> 6) & 0x3F));
      }
      *write++ = static_cast<char>(0x80 | (composed & 0x3F));
      base = composed;
    } else {
      baseStart = write;
      const size_t length = read - start;
      // Preserve uncomposed bytes, including malformed UTF-8: replacement
      // characters could expand the buffer. Earlier compositions may overlap.
      memmove(write, start, length);
      write += length;
      base = utf8IsCombiningMark(cp) ? 0 : cp;
    }
  }
  *write = '\0';
}

bool utf8ContainsLookupCharacter(const char* text) {
  if (!text) return false;
  const auto* cursor = reinterpret_cast<const unsigned char*>(text);
  while (*cursor) {
    if (isLookupCoreCharacter(utf8NextCodepoint(&cursor))) return true;
  }
  return false;
}

bool utf8ContainsLookupCharacter(const std::string& text) { return utf8ContainsLookupCharacter(text.c_str()); }

std::string utf8CleanLookupWord(const std::string& text) {
  const auto* begin = reinterpret_cast<const unsigned char*>(text.c_str());
  const auto* cursor = begin;
  size_t firstCore = std::string::npos;
  size_t lastKeptEnd = 0;

  while (*cursor) {
    const auto* cpStart = cursor;
    const uint32_t cp = utf8NextCodepoint(&cursor);
    if (isLookupCoreCharacter(cp)) {
      if (firstCore == std::string::npos) firstCore = static_cast<size_t>(cpStart - begin);
      lastKeptEnd = static_cast<size_t>(cursor - begin);
    } else if (firstCore != std::string::npos && utf8IsCombiningMark(cp) &&
               static_cast<size_t>(cpStart - begin) == lastKeptEnd) {
      // A trailing mark belongs to the preceding base character. If another
      // core character follows, punctuation between them remains internal.
      lastKeptEnd = static_cast<size_t>(cursor - begin);
    }
  }

  if (firstCore == std::string::npos) return {};
  return utf8ComposeNfc(text.substr(firstCore, lastKeptEnd - firstCore));
}

int utf8CodepointLen(const unsigned char c) {
  if (c < 0x80) return 1;          // 0xxxxxxx
  if ((c >> 5) == 0x6) return 2;   // 110xxxxx
  if ((c >> 4) == 0xE) return 3;   // 1110xxxx
  if ((c >> 3) == 0x1E) return 4;  // 11110xxx
  return 1;                        // fallback for invalid
}

uint32_t utf8NextCodepoint(const unsigned char** string) {
  if (**string == 0) {
    return 0;
  }

  const unsigned char lead = **string;
  const int bytes = utf8CodepointLen(lead);
  const uint8_t* chr = *string;

  // Invalid lead byte (stray continuation byte 0x80-0xBF, or 0xFE/0xFF)
  if (bytes == 1 && lead >= 0x80) {
    (*string)++;
    return REPLACEMENT_GLYPH;
  }

  if (bytes == 1) {
    (*string)++;
    return chr[0];
  }

  // Validate continuation bytes before consuming them
  for (int i = 1; i < bytes; i++) {
    if ((chr[i] & 0xC0) != 0x80) {
      // Missing or invalid continuation byte — skip all bytes consumed so far
      *string += i;
      return REPLACEMENT_GLYPH;
    }
  }

  uint32_t cp = chr[0] & ((1 << (7 - bytes)) - 1);  // mask header bits

  for (int i = 1; i < bytes; i++) {
    cp = (cp << 6) | (chr[i] & 0x3F);
  }

  // Reject overlong encodings, surrogates, and out-of-range values
  const bool overlong = (bytes == 2 && cp < 0x80) || (bytes == 3 && cp < 0x800) || (bytes == 4 && cp < 0x10000);
  const bool surrogate = (cp >= 0xD800 && cp <= 0xDFFF);
  if (overlong || surrogate || cp > 0x10FFFF) {
    (*string)++;
    return REPLACEMENT_GLYPH;
  }

  *string += bytes;

  return cp;
}

void utf8AppendCodepoint(uint32_t cp, std::string& out) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

int utf8SafeTruncateBuffer(const char* buf, int len) {
  if (len <= 0) return 0;

  // Walk back past continuation bytes (10xxxxxx) to find the lead byte
  int leadPos = len - 1;
  while (leadPos > 0 && (static_cast<uint8_t>(buf[leadPos]) & 0xC0) == 0x80) {
    leadPos--;
  }

  // Determine expected length of the sequence starting at leadPos
  int expectedLen = utf8CodepointLen(static_cast<unsigned char>(buf[leadPos]));
  int actualLen = len - leadPos;

  if (actualLen < expectedLen && leadPos > 0) {
    // Incomplete UTF-8 sequence at the end — exclude it
    return leadPos;
  }
  return len;
}

void utf8TrimIncompleteTail(char* buf) {
  if (buf == nullptr) return;
  const size_t len = strlen(buf);
  if (len == 0) return;
  size_t leadPos = len - 1;
  while (leadPos > 0 && (static_cast<unsigned char>(buf[leadPos]) & 0xC0) == 0x80) {
    --leadPos;
  }
  const auto lead = static_cast<unsigned char>(buf[leadPos]);
  // A stray continuation byte at the very start has no lead byte at all.
  const size_t expected = (lead & 0xC0) == 0x80 ? len + 1 : static_cast<size_t>(utf8CodepointLen(lead));
  if (len - leadPos < expected) buf[leadPos] = '\0';
}

bool utf8AppendBounded(std::string& target, const char* src, const size_t len, const size_t maxBytes) {
  if (src == nullptr || len == 0) return false;
  if (target.size() >= maxBytes) return true;
  const size_t remaining = maxBytes - target.size();
  if (len <= remaining) {
    target.append(src, len);
    return false;
  }
  size_t cut = remaining;
  // src[cut] is the first byte left out; if it continues a character, the
  // character started inside the kept part, so leave the whole thing out.
  while (cut > 0 && (static_cast<unsigned char>(src[cut]) & 0xC0) == 0x80) --cut;
  target.append(src, cut);
  return true;
}

void utf8EllipsizeTruncated(std::string& text) {
  const size_t space = text.rfind(' ');
  if (space != std::string::npos && space > 0 && space >= text.size() * 2 / 3) text.resize(space);
  while (!text.empty() && (text.back() == ' ' || text.back() == ',')) text.pop_back();
  text += "\xE2\x80\xA6";  // U+2026 HORIZONTAL ELLIPSIS
}

size_t utf8RemoveLastChar(std::string& str) {
  if (str.empty()) return 0;
  size_t pos = str.size() - 1;
  while (pos > 0 && (static_cast<unsigned char>(str[pos]) & 0xC0) == 0x80) {
    --pos;
  }
  str.resize(pos);
  return pos;
}

// Truncate string by removing N UTF-8 characters from the end
void utf8TruncateChars(std::string& str, const size_t numChars) {
  for (size_t i = 0; i < numChars && !str.empty(); ++i) {
    utf8RemoveLastChar(str);
  }
}

// ---------------------------------------------------------------------------
// Simple case mapping
// ---------------------------------------------------------------------------

// Convert Latin uppercase letters (ASCII plus Latin-1 supplement) to lowercase
uint32_t utf8ToLowerLatin(const uint32_t cp) {
  if (cp >= 'A' && cp <= 'Z') {
    return cp - 'A' + 'a';
  }
  if ((cp >= 0x00C0 && cp <= 0x00D6) || (cp >= 0x00D8 && cp <= 0x00DE)) {
    return cp + 0x20;
  }

  // Latin Extended-A (U+0100..U+017E): uppercase letters are paired with
  // lowercase at cp+1. Two sub-ranges have different alignment:
  //   U+0100..U+0137: uppercase on EVEN codepoints
  //   U+0139..U+0148: uppercase on ODD codepoints
  //   U+014A..U+0177: uppercase on EVEN codepoints
  //   U+0179..U+017E: uppercase on ODD codepoints
  // Covers Polish (Ą/ą, Ć/ć, Ę/ę, Ł/ł, Ń/ń, Ś/ś, Ź/ź, Ż/ż), Czech, Hungarian, Turkish, etc.
  if ((cp >= 0x0100 && cp <= 0x0137 && (cp % 2 == 0)) || (cp >= 0x0139 && cp <= 0x0148 && (cp % 2 == 1)) ||
      (cp >= 0x014A && cp <= 0x0177 && (cp % 2 == 0)) || (cp >= 0x0179 && cp <= 0x017E && (cp % 2 == 1))) {
    return cp + 1;
  }

  switch (cp) {
    case 0x0178:      // Ÿ
      return 0x00FF;  // ÿ
    case 0x1E9E:      // ẞ
      return 0x00DF;  // ß
    default:
      return cp;
  }
}

// Convert Cyrillic uppercase letters to lowercase across U+0400..U+052F.
//   U+0400..U+040F (Ѐ Ё Ђ Ѓ Є Ѕ І Ї Ј Љ Њ Ћ Ќ Ѝ Ў Џ) -> +0x50
//   U+0410..U+042F (А..Я)                            -> +0x20
//   U+0460..U+0481, U+048A..U+04BF, U+04D0..U+052F: case pairs, even = upper (Ѣ, Ґ, Ә, ...)
//   U+04C1..U+04CE: case pairs, odd = upper (Ӂ, ...)
//   U+04C0 (Ӏ palochka) -> U+04CF
uint32_t utf8ToLowerCyrillic(const uint32_t cp) {
  if (cp >= 0x0400 && cp <= 0x040F) {
    return cp + 0x50;
  }
  if (cp >= 0x0410 && cp <= 0x042F) {
    return cp + 0x20;
  }
  if (cp == 0x04C0) {
    return 0x04CF;
  }
  const bool evenUpperBlock =
      (cp >= 0x0460 && cp <= 0x0481) || (cp >= 0x048A && cp <= 0x04BF) || (cp >= 0x04D0 && cp <= 0x052F);
  if (evenUpperBlock && (cp % 2 == 0)) {
    return cp + 1;
  }
  if (cp >= 0x04C1 && cp <= 0x04CE && (cp % 2 == 1)) {
    return cp + 1;
  }
  return cp;
}

uint32_t utf8ToLowerCodepoint(const uint32_t cp) {
  if (cp < 0x80) return (cp >= 'A' && cp <= 'Z') ? cp + ('a' - 'A') : cp;
  if (cp >= 0x0400 && cp <= 0x052F) return utf8ToLowerCyrillic(cp);
  return utf8ToLowerLatin(cp);
}

std::string utf8ToLower(const std::string& in) {
  std::string out;
  bool changed = false;
  const auto* const begin = reinterpret_cast<const unsigned char*>(in.c_str());
  const auto* p = begin;
  while (*p) {
    const auto* start = p;
    const uint32_t cp = utf8NextCodepoint(&p);
    const uint32_t lower = cp == REPLACEMENT_GLYPH ? cp : utf8ToLowerCodepoint(cp);
    if (lower != cp) {
      if (!changed) {
        // Most lookup keys are already lowercase; only copy once something changes.
        out.reserve(in.size());
        out.assign(reinterpret_cast<const char*>(begin), start - begin);
        changed = true;
      }
      utf8AppendCodepoint(lower, out);
    } else if (changed) {
      // Copy the original bytes so malformed UTF-8 passes through unchanged.
      out.append(reinterpret_cast<const char*>(start), p - start);
    }
  }
  return changed ? out : in;
}
