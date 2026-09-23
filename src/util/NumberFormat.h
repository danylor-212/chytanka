#pragma once

// Pure number-text helpers; the separator comes from the caller so this runs
// in host tests.

// Replaces the '.' decimal point that printf emits with the locale's
// separator. Only a '.' between two digits counts as a decimal point, so
// surrounding text (abbreviations, ellipses) is left alone. Only single-byte
// separators are supported; anything else leaves the text unchanged.
inline void applyDecimalSeparator(char* buf, const char* separator) {
  if (buf == nullptr || separator == nullptr || separator[0] == '\0' || separator[1] != '\0' || separator[0] == '.') {
    return;
  }
  const auto isDigit = [](const char c) { return c >= '0' && c <= '9'; };
  for (char* p = buf; *p != '\0'; ++p) {
    if (*p == '.' && p != buf && isDigit(p[-1]) && isDigit(p[1])) *p = separator[0];
  }
}
