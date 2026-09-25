#ifdef CHYTANKA

#include "ChytankaWebI18n.h"

#include <Arduino.h>
#include <I18n.h>
#include <WebServer.h>

#include "html/chytanka/ChytankaI18nEnJs.generated.h"
#include "html/chytanka/ChytankaI18nUkJs.generated.h"

namespace chytanka {

void sendPortalI18n(WebServer& server) {
  const bool uk = I18N.getLanguage() == Language::UK;
  const char* data = uk ? ChytankaI18nUkJs : ChytankaI18nEnJs;
  const size_t size = uk ? ChytankaI18nUkJsCompressedSize : ChytankaI18nEnJsCompressedSize;
  const char* etag = uk ? ChytankaI18nUkJsETag : ChytankaI18nEnJsETag;

  // Same caching as the pages: revalidate every visit. The ETag differs per
  // language, so switching the device language takes effect on reload.
  server.sendHeader("ETag", etag);
  server.sendHeader("Cache-Control", "no-cache");
  if (server.header("If-None-Match") == etag) {
    server.send(304);
    return;
  }
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "application/javascript", data, size);
}

}  // namespace chytanka

#endif  // CHYTANKA
