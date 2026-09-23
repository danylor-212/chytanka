#include "UrlUtils.h"

#include <cctype>
#include <cstdio>
#include <utility>

namespace UrlUtils {
namespace {
bool isHexDigit(const char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }

bool shouldEncode(const unsigned char c) {
  if (c <= 0x20 || c >= 0x7f) return true;
  switch (c) {
    case '"':
    case '<':
    case '>':
    case '\\':
    case '^':
    case '`':
    case '{':
    case '|':
    case '}':
      return true;
    default:
      return false;
  }
}
}  // namespace

bool isHttpsUrl(const std::string& url) { return url.rfind("https://", 0) == 0; }

std::string ensureProtocol(const std::string& url) {
  if (url.find("://") == std::string::npos) {
    return "http://" + url;
  }
  return url;
}

std::string extractHost(const std::string& url) {
  const size_t protocolEnd = url.find("://");
  if (protocolEnd == std::string::npos) {
    // No protocol, find first slash
    const size_t firstSlash = url.find('/');
    return firstSlash == std::string::npos ? url : url.substr(0, firstSlash);
  }
  // Find the first slash after the protocol
  const size_t hostStart = protocolEnd + 3;
  const size_t pathStart = url.find('/', hostStart);
  return pathStart == std::string::npos ? url : url.substr(0, pathStart);
}

std::string encodeUnsafeUrlChars(const std::string& url) {
  std::string out;
  out.reserve(url.size());
  for (size_t i = 0; i < url.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(url[i]);
    if (c == '%' && i + 2 < url.size() && isHexDigit(url[i + 1]) && isHexDigit(url[i + 2])) {
      out += url[i];
      out += url[i + 1];
      out += url[i + 2];
      i += 2;
    } else if (c == '%' || shouldEncode(c)) {
      char encoded[4];
      snprintf(encoded, sizeof(encoded), "%%%02X", c);
      out += encoded;
    } else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

namespace {

bool hasScheme(const std::string& ref) {
  // RFC 3986 scheme: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) ":"
  if (ref.empty() || !std::isalpha(static_cast<unsigned char>(ref[0]))) return false;
  for (size_t i = 1; i < ref.size(); ++i) {
    const char c = ref[i];
    if (c == ':') return true;
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' && c != '.') return false;
  }
  return false;
}

// RFC 3986 section 5.2.4 remove_dot_segments, for a path without query.
std::string removeDotSegments(const std::string& path) {
  std::string output;
  size_t i = 0;
  while (i < path.size()) {
    if (path.compare(i, 3, "../") == 0) {
      i += 3;
    } else if (path.compare(i, 2, "./") == 0) {
      i += 2;
    } else if (path.compare(i, 3, "/./") == 0) {
      i += 2;
    } else if (i + 2 == path.size() && path.compare(i, 2, "/.") == 0) {
      output += '/';
      break;
    } else if (path.compare(i, 4, "/../") == 0 || (i + 3 == path.size() && path.compare(i, 3, "/..") == 0)) {
      const bool last = path.compare(i, 4, "/../") != 0;
      const size_t slash = output.rfind('/');
      output.resize(slash == std::string::npos ? 0 : slash);
      if (last) {
        output += '/';
        break;
      }
      i += 3;
    } else if ((i + 1 == path.size() && path[i] == '.') || (i + 2 == path.size() && path.compare(i, 2, "..") == 0)) {
      break;
    } else {
      const size_t next = path.find('/', i + 1);
      const size_t segmentEnd = next == std::string::npos ? path.size() : next;
      output.append(path, i, segmentEnd - i);
      i = segmentEnd;
    }
  }
  return output;
}

// Splits "path?query#fragment" into the path and the "?query" suffix (the
// fragment is dropped: it never reaches the server).
void splitPathAndQuery(const std::string& ref, std::string& path, std::string& query) {
  std::string withoutFragment = ref.substr(0, ref.find('#'));
  const size_t queryPos = withoutFragment.find('?');
  if (queryPos == std::string::npos) {
    path = std::move(withoutFragment);
    query.clear();
  } else {
    path = withoutFragment.substr(0, queryPos);
    query = withoutFragment.substr(queryPos);
  }
}

}  // namespace

std::string buildUrl(const std::string& serverUrl, const std::string& path) {
  // RFC 3986 section 5.2 reference resolution for the reference forms OPDS
  // feeds use. `serverUrl` is the base (the feed the href came from).
  if (hasScheme(path)) {
    return encodeUnsafeUrlChars(path);
  }
  const std::string base = ensureProtocol(serverUrl);
  if (path.empty()) {
    return encodeUnsafeUrlChars(base);
  }
  const size_t schemeEnd = base.find("://");
  if (path.compare(0, 2, "//") == 0) {
    // Scheme-relative: keep the base's scheme.
    return encodeUnsafeUrlChars(base.substr(0, schemeEnd + 1) + path);
  }

  const std::string origin = extractHost(base);  // scheme://authority
  std::string basePath;
  std::string baseQuery;
  splitPathAndQuery(base.substr(origin.size()), basePath, baseQuery);
  if (basePath.empty()) basePath = "/";

  if (path[0] == '?') {
    return encodeUnsafeUrlChars(origin + basePath + path.substr(0, path.find('#')));
  }
  if (path[0] == '#') {
    return encodeUnsafeUrlChars(origin + basePath + baseQuery);
  }

  std::string refPath;
  std::string refQuery;
  splitPathAndQuery(path, refPath, refQuery);
  std::string merged;
  if (refPath[0] == '/') {
    merged = refPath;
  } else {
    // Path-relative: resolve against the base's directory (everything up to
    // and including its last '/'), so ".../opds/index.xml" + "books/a.epub"
    // becomes ".../opds/books/a.epub".
    merged = basePath.substr(0, basePath.rfind('/') + 1) + refPath;
  }
  return encodeUnsafeUrlChars(origin + removeDotSegments(merged) + refQuery);
}

}  // namespace UrlUtils
