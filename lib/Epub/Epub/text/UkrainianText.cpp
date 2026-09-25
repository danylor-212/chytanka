#include "UkrainianText.h"

#include <cstring>

namespace ukrainian_text {
namespace {

constexpr uint32_t APOSTROPHE = 0x02BC;
constexpr uint32_t NBSP = 0x00A0;
constexpr uint32_t EM_DASH = 0x2014;
constexpr uint32_t EN_DASH = 0x2013;
constexpr uint32_t LEFT_GUILLEMET = 0x00AB;
constexpr uint32_t RIGHT_GUILLEMET = 0x00BB;

bool isCyrillicLetter(const uint32_t cp) { return (cp >= 0x0400 && cp <= 0x04FF && cp != 0x0482) || cp == 0x0500; }

bool isUkrainianOnlyLetter(const uint32_t cp) {
  switch (cp) {
    case 0x0490:  // Ґ
    case 0x0491:  // ґ
    case 0x0404:  // Є
    case 0x0454:  // є
    case 0x0406:  // І
    case 0x0456:  // і
    case 0x0407:  // Ї
    case 0x0457:  // ї
      return true;
    default:
      return false;
  }
}

bool isRussianOnlyLetter(const uint32_t cp) {
  switch (cp) {
    case 0x042B:  // Ы
    case 0x044B:  // ы
    case 0x042D:  // Э
    case 0x044D:  // э
    case 0x042A:  // Ъ
    case 0x044A:  // ъ
    case 0x0401:  // Ё
    case 0x0451:  // ё
      return true;
    default:
      return false;
  }
}

bool isLatinLetter(const uint32_t cp) {
  return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
         (cp >= 0x00C0 && cp <= 0x024F && cp != 0x00D7 && cp != 0x00F7);
}

bool isSpace(const uint32_t cp) { return cp == ' ' || cp == '\n' || cp == '\r' || cp == '\t'; }

bool isOneLetterWord(const uint32_t cp) {
  switch (cp) {
    case 0x0432:  // в
    case 0x0412:  // В
    case 0x0443:  // у
    case 0x0423:  // У
    case 0x0437:  // з
    case 0x0417:  // З
    case 0x0456:  // і
    case 0x0406:  // І
    case 0x0439:  // й
    case 0x0419:  // Й
    case 0x0430:  // а
    case 0x0410:  // А
    case 0x043E:  // о
    case 0x041E:  // О
      return true;
    default:
      return false;
  }
}

bool isApostropheLike(const uint32_t cp) { return cp == '\'' || cp == 0x2019 || cp == 0x2018 || cp == '`'; }

// A straight double quote after one of these (or at a paragraph start) opens.
bool opensQuote(const uint32_t before) {
  return before == 0 || isSpace(before) || before == NBSP || before == '(' || before == '[' || before == '{' ||
         before == EM_DASH || before == EN_DASH || before == '-' || before == LEFT_GUILLEMET || before == 0x201E ||
         before == 0x201C;
}

bool isWordCharacter(const uint32_t cp) {
  return isCyrillicLetter(cp) || isLatinLetter(cp) || (cp >= '0' && cp <= '9') || cp == APOSTROPHE;
}

// Decodes one codepoint; malformed bytes decode as themselves so the
// codepoint count stays the same as the parser's own counting.
uint32_t decode(const unsigned char*& p, const unsigned char* end) {
  const unsigned char c = *p++;
  if (c < 0x80) return c;
  int extra = 0;
  uint32_t cp = 0;
  if ((c & 0xE0) == 0xC0) {
    extra = 1;
    cp = c & 0x1F;
  } else if ((c & 0xF0) == 0xE0) {
    extra = 2;
    cp = c & 0x0F;
  } else if ((c & 0xF8) == 0xF0) {
    extra = 3;
    cp = c & 0x07;
  } else {
    return 0xFFFD;
  }
  for (int i = 0; i < extra; ++i) {
    if (p >= end || (*p & 0xC0) != 0x80) return 0xFFFD;
    cp = (cp << 6) | (*p++ & 0x3F);
  }
  return cp;
}

void encode(const uint32_t cp, std::string& out) {
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

constexpr const char* SKIPPED_ELEMENTS[] = {"head", "style", "script"};

}  // namespace

bool isUkrainianLanguageTag(const std::string& tag) {
  if (tag.size() < 2) return false;
  const char a = static_cast<char>(tag[0] | 0x20);
  const char b = static_cast<char>(tag[1] | 0x20);
  return a == 'u' && b == 'k' && (tag.size() == 2 || tag[2] == '-' || tag[2] == '_');
}

void LanguageSniffer::finishTagName() {
  readingTagName_ = false;
  tagName_[tagNameLen_] = '\0';
  for (uint8_t i = 0; i < 3; ++i) {
    if (strcmp(tagName_, SKIPPED_ELEMENTS[i]) != 0) continue;
    if (closingTag_) {
      if (skipElement_ == i + 1) skipElement_ = 0;
    } else if (skipElement_ == 0) {
      skipElement_ = static_cast<uint8_t>(i + 1);
    }
  }
}

void LanguageSniffer::feed(const char* data, const size_t len) {
  for (size_t i = 0; i < len && !full(); ++i) {
    const auto c = static_cast<unsigned char>(data[i]);
    if (inTag_) {
      if (readingTagName_) {
        if (c == '/' && tagNameLen_ == 0 && !closingTag_) {
          closingTag_ = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
          if (tagNameLen_ < sizeof(tagName_) - 1) tagName_[tagNameLen_++] = static_cast<char>(c | 0x20);
        } else {
          finishTagName();
        }
      }
      if (c == '>') {
        if (readingTagName_) finishTagName();
        inTag_ = false;
      }
      continue;
    }
    if (c == '<') {
      inTag_ = true;
      readingTagName_ = true;
      closingTag_ = false;
      tagNameLen_ = 0;
      pendingBytes_ = 0;
      continue;
    }
    if (skipElement_ != 0) continue;
    if (inEntity_) {
      if (c == ';' || c == ' ' || c == '<') inEntity_ = false;
      continue;
    }
    if (c == '&') {
      inEntity_ = true;
      continue;
    }
    ++visibleBytes_;
    if (pendingBytes_ > 0) {
      if ((c & 0xC0) == 0x80) {
        pendingCp_ = (pendingCp_ << 6) | (c & 0x3F);
        if (--pendingBytes_ == 0) countCodepoint(pendingCp_);
        continue;
      }
      pendingBytes_ = 0;
    }
    if (c < 0x80) {
      countCodepoint(c);
    } else if ((c & 0xE0) == 0xC0) {
      pendingCp_ = c & 0x1F;
      pendingBytes_ = 1;
    } else if ((c & 0xF0) == 0xE0) {
      pendingCp_ = c & 0x0F;
      pendingBytes_ = 2;
    } else if ((c & 0xF8) == 0xF0) {
      pendingCp_ = c & 0x07;
      pendingBytes_ = 3;
    }
  }
}

void LanguageSniffer::countCodepoint(const uint32_t cp) {
  if (isCyrillicLetter(cp)) {
    ++cyrillic_;
    if (isUkrainianOnlyLetter(cp)) ++ukrainian_;
    if (isRussianOnlyLetter(cp)) ++russian_;
  } else if (isLatinLetter(cp)) {
    ++latin_;
  }
}

bool LanguageSniffer::looksUkrainian() const {
  if (cyrillic_ < MIN_CYRILLIC) return false;
  if (cyrillic_ * 10 < (cyrillic_ + latin_) * 6) return false;  // Cyrillic < 60% of letters
  if (ukrainian_ * 50 < cyrillic_) return false;                // ґєії < 2% of Cyrillic
  return ukrainian_ >= 2 * russian_;
}

void Typography::apply(const char* text, const size_t len, std::string& out) {
  out.clear();
  out.reserve(len + len / 2);
  const auto* p = reinterpret_cast<const unsigned char*>(text);
  const auto* const end = p + len;
  // Decode one codepoint of look-ahead at a time.
  const unsigned char* nextPtr = p;
  uint32_t cur = p < end ? decode(nextPtr, end) : 0;
  bool haveCur = p < end;
  while (haveCur) {
    const unsigned char* afterNext = nextPtr;
    const bool haveNext = nextPtr < end;
    const uint32_t next = haveNext ? decode(afterNext, end) : 0;

    uint32_t outCp = cur;
    if (isApostropheLike(cur)) {
      if (isCyrillicLetter(prev_) && haveNext && isCyrillicLetter(next)) outCp = APOSTROPHE;
    } else if (cur == '"') {
      outCp = opensQuote(prev_) ? LEFT_GUILLEMET : RIGHT_GUILLEMET;
    } else if (cur == '-') {
      // Spaced hyphen, or a dialogue dash opening the paragraph ("- Ти прийдеш?").
      if ((isSpace(prev_) || prev_ == NBSP || prev_ == 0) && haveNext && isSpace(next)) outCp = EM_DASH;
    } else if (isSpace(cur) && haveNext) {
      const bool afterOneLetterWord = isOneLetterWord(prev_) && !isWordCharacter(prevPrev_) && prevPrev_ != '-';
      const bool beforeDash = next == EM_DASH || (next == '-' && [&] {
                                const unsigned char* q = afterNext;
                                return q < end && isSpace(decode(q, end));
                              }());
      // The space after a paragraph-opening dialogue dash keeps the dash on the line.
      const bool afterOpeningDash = prev_ == EM_DASH && prevPrev_ == 0;
      if ((afterOneLetterWord && !isSpace(next)) || beforeDash || (afterOpeningDash && !isSpace(next))) outCp = NBSP;
    }
    encode(outCp, out);

    prevPrev_ = prev_;
    prev_ = outCp;
    cur = next;
    nextPtr = afterNext;
    haveCur = haveNext;
  }
}

}  // namespace ukrainian_text
