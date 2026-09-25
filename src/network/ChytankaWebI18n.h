#pragma once

// Fork-only («Читанка» / Chytanka): serves /i18n.js for the web portal, the
// Ukrainian or English table plus the in-page translator
// (web/chytanka/{uk,en,i18n}.js, built by scripts/chytanka/build_web_chytanka.py).
// Only compiled in when the build defines CHYTANKA.

class WebServer;

namespace chytanka {

// Picks the table from the device's UI language at request time. The pages
// themselves are static and identical for both languages.
void sendPortalI18n(WebServer& server);

}  // namespace chytanka
