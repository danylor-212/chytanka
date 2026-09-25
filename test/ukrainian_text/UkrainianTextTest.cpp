#include <gtest/gtest.h>

#include <string>

#include "UkrainianText.h"

using ukrainian_text::isUkrainianLanguageTag;
using ukrainian_text::LanguageSniffer;
using ukrainian_text::Typography;

namespace {

size_t codepoints(const std::string& s) {
  size_t n = 0;
  for (const char c : s) n += (static_cast<unsigned char>(c) & 0xC0) != 0x80;
  return n;
}

std::string typeset(const std::string& in) {
  Typography t;
  std::string out;
  t.apply(in.data(), in.size(), out);
  EXPECT_EQ(codepoints(out), codepoints(in)) << in;
  return out;
}

bool sniff(const std::string& xhtml) {
  LanguageSniffer s;
  s.feed(xhtml.data(), xhtml.size());
  return s.looksUkrainian();
}

const char* const kUkrainian =
    "Було колись — в Україні ревіли гармати; було колись — запорожці вміли панувати. Панували, добували і славу, "
    "і волю; минулося — осталися могили на полі. Високії ті могили, де лягло спочити козацькеє біле тіло, в "
    "китайку повите. Їхав козак на війноньку, сказав: «Прощай, мила!» Єдиний шлях — ґанок, їжак, єнот, Європа.";
const char* const kRussian =
    "Все счастливые семьи похожи друг на друга, каждая несчастливая семья несчастлива по-своему. Всё смешалось в "
    "доме Облонских. Жена узнала, что муж был в связи с бывшею в их доме француженкою-гувернанткой, и объявила "
    "мужу, что не может жить с ним в одном доме. Положение это продолжалось уже третий день и мучительно "
    "чувствовалось и самими супругами, и всеми членами семьи, и домочадцами. Дети бегали по всему дому, как "
    "потерянные; англичанка поссорилась с экономкой; повар ушёл ещё вчера со двора, во время обеда.";
const char* const kRussianPreReform =
    "Всѣ счастливыя семьи похожи другъ на друга, каждая несчастливая семья несчастлива по-своему. Все смѣшалось "
    "въ домѣ Облонскихъ. Жена узнала, что мужъ былъ въ связи съ бывшею въ ихъ домѣ француженкою-гувернанткой, и "
    "объявила мужу, что не можетъ жить съ нимъ въ одномъ домѣ. Положеніе это продолжалось уже третій день и "
    "мучительно чувствовалось и самими супругами, и всѣми членами семьи и домочадцами.";
const char* const kBelarusian =
    "Беларуская мова — нацыянальная мова беларусаў, адна з дзвюх дзяржаўных моў Рэспублікі Беларусь. Яна "
    "належыць да ўсходнеславянскай групы індаеўрапейскай моўнай сям'і. Пашырана ў асноўным у Беларусі, а "
    "таксама ў суседніх краінах. Сучасная літаратурная мова сфарміравалася ў канцы дзевятнаццатага і пачатку "
    "дваццатага стагоддзя на аснове сярэднебеларускіх гаворак.";

}  // namespace

TEST(UkrainianLanguageTag, PrimarySubtag) {
  EXPECT_TRUE(isUkrainianLanguageTag("uk"));
  EXPECT_TRUE(isUkrainianLanguageTag("uk-UA"));
  EXPECT_TRUE(isUkrainianLanguageTag("UK"));
  EXPECT_FALSE(isUkrainianLanguageTag("ukr-x"));
  EXPECT_FALSE(isUkrainianLanguageTag("ru"));
  EXPECT_FALSE(isUkrainianLanguageTag(""));
  EXPECT_FALSE(isUkrainianLanguageTag("und"));
}

TEST(LanguageSniffer, DetectsUkrainianProse) {
  EXPECT_TRUE(sniff(std::string("<html><head><title>x</title><style>p{font-family:serif}</style></head><body><p>") +
                    kUkrainian + "</p></body></html>"));
}

TEST(LanguageSniffer, NeverCallsRussianOrBelarusianUkrainian) {
  EXPECT_FALSE(sniff(std::string("<p>") + kRussian + "</p>"));
  EXPECT_FALSE(sniff(std::string("<p>") + kRussianPreReform + "</p>"));
  EXPECT_FALSE(sniff(std::string("<p>") + kBelarusian + "</p>"));
}

TEST(LanguageSniffer, EnglishAndShortTextAreNotUkrainian) {
  std::string english = "<p>";
  for (int i = 0; i < 20; ++i) english += "It was the best of times, it was the worst of times. ";
  english += "Київ, Україна, їжак.</p>";
  EXPECT_FALSE(sniff(english));
  EXPECT_FALSE(sniff("<p>Їжак і єнот.</p>"));  // fewer than 100 Cyrillic letters
}

TEST(LanguageSniffer, IgnoresMarkupEntitiesAndSkippedElements) {
  LanguageSniffer s;
  const std::string html =
      "<head><title>Їїїїїї</title><script>var ї='ї';</script></head><body class=\"їжак\"><p>abc&#x457;&laquo;d</p>";
  s.feed(html.data(), html.size());
  EXPECT_EQ(s.cyrillicLetters(), 0u);
  EXPECT_EQ(s.latinLetters(), 4u);
}

TEST(LanguageSniffer, SplitAcrossChunksAndStopsAtLimit) {
  const std::string text = std::string("<p>") + kUkrainian + "</p>";
  LanguageSniffer s;
  for (size_t i = 0; i < text.size(); i += 3) s.feed(text.data() + i, std::min<size_t>(3, text.size() - i));
  EXPECT_TRUE(s.looksUkrainian());

  LanguageSniffer small(40);
  small.feed(text.data(), text.size());
  EXPECT_TRUE(small.full());
  EXPECT_LE(small.cyrillicLetters(), 40u);
}

TEST(Typography, ApostropheBetweenCyrillicLetters) {
  EXPECT_EQ(typeset("м'ята, п’ять, об‘єднання, сім`я"), "мʼята, пʼять, обʼєднання, сімʼя");
  EXPECT_EQ(typeset("don't, I'm, O’Brien"), "don't, I'm, O’Brien");
  // Not between two letters (the space after "і" is a no-break space rule).
  EXPECT_EQ(typeset("'слово' і ’цитата’"), "'слово' і\xC2\xA0’цитата’");
}

TEST(Typography, StraightQuotesByPosition) {
  EXPECT_EQ(typeset("Він сказав: \"Так\"."), "Він сказав: «Так».");
  EXPECT_EQ(typeset("\"Слово\""), "«Слово»");
  EXPECT_EQ(typeset("(\"a\") — \"b\""), "(«a»)\xC2\xA0— «b»");
  // Existing typographic quotes stay as they are.
  EXPECT_EQ(typeset("«Так», “yes”, „ні“"), "«Так», “yes”, „ні“");
}

TEST(Typography, QuoteAtParagraphEndAndAcrossChunks) {
  Typography t;
  std::string out;
  std::string all;
  const std::string first = "Кажу \"до";
  const std::string second = "брий\"";
  t.apply(first.data(), first.size(), out);
  all += out;
  t.apply(second.data(), second.size(), out);
  all += out;
  EXPECT_EQ(all, "Кажу «добрий»");
  t.resetBlock();
  const std::string third = "\"Новий\"";
  t.apply(third.data(), third.size(), out);
  EXPECT_EQ(out, "«Новий»");
}

TEST(Typography, SpacedHyphenBecomesDashWithNoBreakBefore) {
  EXPECT_EQ(typeset("Київ - столиця"), "Київ\xC2\xA0— столиця");
  EXPECT_EQ(typeset("Київ — столиця"), "Київ\xC2\xA0— столиця");
  EXPECT_EQ(typeset("що-небудь, будь-хто"), "що-небудь, будь-хто");
  EXPECT_EQ(typeset("2 - 3"), "2\xC2\xA0— 3");
}

TEST(Typography, DialogueDashOpeningAParagraph) {
  EXPECT_EQ(typeset("- Ти прийдеш? - спитала вона."), "—\xC2\xA0Ти прийдеш?\xC2\xA0— спитала вона.");
  // Only at the paragraph start: after other text a lone "-" needs spaces around it.
  Typography t;
  std::string out;
  const std::string first = "Слово";
  t.apply(first.data(), first.size(), out);
  const std::string second = "- ні";
  t.apply(second.data(), second.size(), out);
  EXPECT_EQ(out, "- ні");
  // "-5" or "-то" at a paragraph start is not a dialogue dash.
  EXPECT_EQ(typeset("-5 градусів"), "-5 градусів");
}

TEST(Typography, NoBreakSpaceAfterOneLetterWords) {
  EXPECT_EQ(typeset("Я пішов у ліс і в поле, а там з другом."),
            "Я пішов у\xC2\xA0ліс і\xC2\xA0в\xC2\xA0поле, а\xC2\xA0там з\xC2\xA0другом.");
  EXPECT_EQ(typeset("В лісі й О там"), "В\xC2\xA0лісі й\xC2\xA0О\xC2\xA0там");
  // Not one-letter words: the letter ends a longer word or follows a hyphen.
  EXPECT_EQ(typeset("вона пішла, хтось-з там"), "вона пішла, хтось-з там");
  // A trailing space (look-ahead unknown or end of text) is left alone.
  EXPECT_EQ(typeset("і "), "і ");
}

TEST(Typography, CodepointCountNeverChanges) {
  const std::string mixed = std::string(kUkrainian) + " \"'` - ' - \" ' і в у й";
  typeset(mixed);
  typeset(kRussian);
  typeset("abc \"x\" y - z don't");
}
