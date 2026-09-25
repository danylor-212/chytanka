// Fork-only («Читанка» / Chytanka): English table for i18n.js. The portal is
// already English; this only keeps CrossInk's own name out of what the page
// script writes at runtime (the Chytanka pages' static chrome is branded at
// build time).
window.CHYTANKA_I18N = {
  lang: "en",
  dict: {},
  patterns: [[/^(.+) - Files - CrossInk Reader$/, "$1 · Files · Читанка"]],
};
