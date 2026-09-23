#pragma once
#include <string>

namespace UrlUtils {

/**
 * Check if URL uses HTTPS protocol
 */
bool isHttpsUrl(const std::string& url);

/**
 * Prepend http:// if no protocol specified (server will redirect to https if needed)
 */
std::string ensureProtocol(const std::string& url);

/**
 * Extract host with protocol from URL (e.g., "http://example.com" from "http://example.com/path")
 */
std::string extractHost(const std::string& url);

/**
 * Percent-encode raw characters that esp_http_client rejects in a URL.
 */
std::string encodeUnsafeUrlChars(const std::string& url);

/**
 * Resolve `path` (an href from a feed) against the base URL `serverUrl`, as
 * RFC 3986 section 5.2 does: absolute URLs are kept, "//host/..." takes the
 * base's scheme, "/path" is relative to the host root, "?query" replaces the
 * base's query, and anything else is relative to the base's directory, with
 * "." and ".." segments resolved. Queries in `path` are kept. A base without
 * a scheme is treated as http://.
 */
std::string buildUrl(const std::string& serverUrl, const std::string& path);

}  // namespace UrlUtils
