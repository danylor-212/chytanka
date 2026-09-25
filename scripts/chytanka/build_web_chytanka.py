"""Fork-only («Читанка» / Chytanka): Chytanka variants of the web portal.

Runs as a PlatformIO pre-script of the Chytanka envs only
(platformio.chytanka.ini), after CrossInk's scripts/build_web.py, and reuses
that script's page composition, minifier and header writer, so the stock
headers (and every non-Chytanka build) stay exactly as CrossInk makes them.

Writes src/network/html/chytanka/ (git-ignored, like all *.generated.h):
  <Page>Html.generated.h  - the four portal pages with the same identifiers as
                            the stock headers, composed from the same
                            web/templates/base.html and web/pages/*, with
                            * the «Читанка» logo (inline SVG) and wordmark in
                              the header instead of CrossInk's,
                            * a "based on CrossInk" footer,
                            * <script src="/i18n.js"> before the page script;
  ChytankaI18nUkJs / ChytankaI18nEnJs.generated.h
                          - web/chytanka/{uk,en}.js + web/chytanka/i18n.js,
                            served at /i18n.js by the language the device UI
                            is set to (src/network/ChytankaWebI18n.cpp).

CrossPointWebServer.cpp includes the chytanka/ page headers instead of the
stock ones under #ifdef CHYTANKA. If CrossInk changes the header or footer
markup in base.html, the replacements below fail loudly instead of silently
shipping CrossInk's chrome."""
import importlib.util
import os
import re
import sys

try:
    ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
except NameError:
    ROOT = os.getcwd()  # PlatformIO may exec pre-scripts without __file__

WEB = os.path.join(ROOT, "web")
CHY = os.path.join(WEB, "chytanka")
OUT = os.path.join(ROOT, "src", "network", "html", "chytanka")

spec = importlib.util.spec_from_file_location("crossink_build_web", os.path.join(ROOT, "scripts", "build_web.py"))
bw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bw)  # also (re)writes the stock headers, unchanged

TITLES = {
    "home": "Читанка",
    "files": "Files · Читанка",
    "settings": "Settings · Читанка",
    "fonts": "Fonts · Читанка",
}

STOCK_H1 = re.compile(r"<h1>.*?</h1>", re.DOTALL)
STOCK_FOOTER = '<p class="footer">CrossInk &bull; Open Source</p>'
SCRIPT_SLOT = "{{ script }}"


def fail(msg):
    sys.stderr.write(f"build_web_chytanka: {msg}\n")
    raise SystemExit(1)


def chytanka_base(base):
    logo = bw.read(CHY, "logo.svg").strip()
    h1 = f'<h1>{logo}<span class="wordmark">Читанка</span></h1>'
    if len(STOCK_H1.findall(base)) != 1:
        fail("expected exactly one <h1> in web/templates/base.html")
    if base.count(STOCK_FOOTER) != 1 or base.count(SCRIPT_SLOT) != 1:
        fail("web/templates/base.html footer or {{ script }} slot changed; update this script")
    base = STOCK_H1.sub(lambda _: h1, base)
    base = base.replace(STOCK_FOOTER, '<p class="footer">Читанка · based on CrossInk · Open Source</p>')
    return base.replace(SCRIPT_SLOT, '<script src="/i18n.js"></script>\n' + SCRIPT_SLOT)


def strip_js(js):
    # Drop whole-line comments and indentation only; the sources carry no
    # "//" inside strings or regexes at the start of a line.
    lines = (line.strip() for line in js.splitlines())
    return "\n".join(line for line in lines if line and not line.startswith("//"))


os.makedirs(OUT, exist_ok=True)
base = chytanka_base(bw.read(WEB, "templates", "base.html"))
total = 0
for slug, (ident, _title, active, head_extra) in bw.PAGES.items():
    page_js = bw.read(WEB, "pages", f"{slug}.js").strip()
    values = {
        "title": TITLES[slug], "v": bw.v, "head_extra": head_extra,
        "styles": bw.read(WEB, "pages", f"{slug}.css"), "body": bw.read(WEB, "pages", f"{slug}.html"),
        "script": f"<script>\n{page_js}\n</script>" if page_js else "",
        "cls_home": "", "cls_files": "", "cls_settings": "", "cls_fonts": "",
    }
    values[f"cls_{active}"] = ' class="active"'
    html = bw.minify_html(bw.render(base, values))
    orig, comp = bw.emit_gzip(os.path.join(OUT, f"{ident}.generated.h"), ident, html)
    total += comp
    print(f"chytanka/{ident:18} {orig:>7}B -> {comp:>6}B gz")

engine = strip_js(bw.read(CHY, "i18n.js"))
for lang, ident in (("uk", "ChytankaI18nUkJs"), ("en", "ChytankaI18nEnJs")):
    js = strip_js(bw.read(CHY, f"{lang}.js")) + "\n" + engine + "\n"
    orig, comp = bw.emit_gzip(os.path.join(OUT, f"{ident}.generated.h"), ident, js)
    total += comp
    print(f"chytanka/{ident:18} {orig:>7}B -> {comp:>6}B gz")
print(f"chytanka web portal: {total}B gz in flash")
