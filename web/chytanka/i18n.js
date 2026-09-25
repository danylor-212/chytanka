// Fork-only («Читанка» / Chytanka): translates the web portal in the browser.
//
// The portal pages are CrossInk's English HTML/JS. The device serves this
// script at /i18n.js, prefixed with one language table (uk.js or en.js, picked
// from the device's UI language when the request arrives), and every
// Chytanka page loads it just before its own script. It then
//   - translates the text already in the page and every text node or
//     placeholder/title/aria-label the page's script adds later
//     (MutationObserver), by exact match on the whitespace-trimmed text, or
//     on the text after a leading emoji/symbol prefix ("📤 Upload");
//   - falls back to the table's regular expressions for messages with
//     numbers or names in them;
//   - wraps alert/confirm/prompt so their messages are translated too.
// File, folder, font and network names are left alone (NO_TRANSLATE below).
// Anything not in the table stays in English. Built by
// scripts/chytanka/build_web_chytanka.py; see docs/chytanka/RELEASING.md.
(function () {
  "use strict";
  var data = window.CHYTANKA_I18N;
  if (!data) return;
  if (data.lang) document.documentElement.lang = data.lang;
  var dict = data.dict || {};
  var patterns = data.patterns || [];
  var has = Object.prototype.hasOwnProperty;

  // User content: never translated even when it happens to match a key.
  var NO_TRANSLATE =
    "script,style,textarea,pre,code,datalist,.file-link,.folder-link,.breadcrumb-inline," +
    "#deleteItemList,#renameItemName,#moveItemName,#imagePreviewName,#uploadPathDisplay," +
    "#folderPathDisplay,.failed-file-name,.family-info h3,.log-container,[data-no-i18n]";
  var ATTRS = ["placeholder", "title", "aria-label"];

  function lookup(text) {
    if (has.call(dict, text)) return dict[text];
    for (var i = 0; i < patterns.length; i++) {
      var p = patterns[i];
      p[0].lastIndex = 0;
      if (p[0].test(text)) return text.replace(p[0], p[1]);
    }
    return null;
  }

  function translate(text) {
    if (!text || !/[A-Za-z]/.test(text)) return null;
    var m = /^(\s*)([\s\S]*?)(\s*)$/.exec(text);
    var body = m[2].replace(/\s+/g, " ");
    var out = lookup(body);
    if (out === null) {
      // "📤 Upload", "⚠️ Some files failed to upload", "+ Add Network"
      var pm = /^([^A-Za-z0-9]+)([A-Za-z][\s\S]*)$/.exec(body);
      if (pm) {
        var t = lookup(pm[2]);
        if (t !== null) out = pm[1] + t;
      }
    }
    return out === null || out === body ? null : m[1] + out + m[3];
  }

  function translateLines(text) {
    if (typeof text !== "string") return text;
    return text
      .split("\n")
      .map(function (line) {
        var t = translate(line);
        return t === null ? line : t;
      })
      .join("\n");
  }

  function skipped(el) {
    return !el || (el.closest && el.closest(NO_TRANSLATE) !== null);
  }

  function fixText(node) {
    if (skipped(node.parentElement)) return;
    var t = translate(node.nodeValue);
    if (t !== null) node.nodeValue = t;
  }

  function fixAttrs(el) {
    if (skipped(el)) return;
    for (var i = 0; i < ATTRS.length; i++) {
      var v = el.getAttribute(ATTRS[i]);
      if (v) {
        var t = translate(v);
        if (t !== null) el.setAttribute(ATTRS[i], t);
      }
    }
  }

  function walk(root) {
    if (root.nodeType === 3) return fixText(root);
    if (root.nodeType !== 1 || skipped(root)) return;
    fixAttrs(root);
    var sel = root.querySelectorAll("[placeholder],[title],[aria-label]");
    for (var i = 0; i < sel.length; i++) fixAttrs(sel[i]);
    var it = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, null);
    var n;
    while ((n = it.nextNode())) fixText(n);
  }

  walk(document.documentElement);
  new MutationObserver(function (mutations) {
    mutations.forEach(function (m) {
      if (m.type === "childList") {
        for (var i = 0; i < m.addedNodes.length; i++) walk(m.addedNodes[i]);
      } else if (m.type === "characterData") {
        fixText(m.target);
      } else if (m.type === "attributes") {
        fixAttrs(m.target);
      }
    });
  }).observe(document.documentElement, {
    childList: true,
    subtree: true,
    characterData: true,
    attributes: true,
    attributeFilter: ATTRS,
  });

  ["alert", "confirm", "prompt"].forEach(function (name) {
    var original = window[name];
    if (typeof original !== "function") return;
    window[name] = function (message) {
      var args = Array.prototype.slice.call(arguments);
      args[0] = translateLines(message);
      return original.apply(window, args);
    };
  });
})();
