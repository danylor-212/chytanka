# Rejected candidates — public-domain set

Candidates dropped from `quotes.json`, with the reason. "Not found" means the wording could not be matched verbatim in the full text we fetched.

## Not found verbatim / misquoted forms
- **Шевченко, «І мертвим, і живим…»**: «Учітеся, брати мої, / Думайте, читайте, / І чужому научайтесь…». This popular form merges two passages. The poem reads «Не дуріте самі себе! / Учітесь, читайте, / І чужому научайтесь, / Й свого не цурайтесь.», and that text is what we used (id in quotes.json). «Учітеся, брати мої» turns up only inside other authors' texts that quote Shevchenko (Грінченко «Під тихими вербами», Мирний «На 37-мі роковини…»).
- **Шевченко, «Кавказ»**: «…Вам Бог помагає!» with a capital «Б» is not in the fetched edition, which has «Вам бог помагає!». We originally kept the quote exactly as the ukrlib.com.ua source prints it. 2026-09-23 re-verification against the modern academic edition removed the entry entirely rather than keep the lowercase form — see "Re-verification of «бог»/«Бог» capitalisation (2026-09-23)" below for the full finding.
- **Леся Українка**: «Хто визволиться сам, той буде вільний…», often attributed to her online. Not found in any of the 208 fetched Lesya Ukrainka texts, so rejected.

## Source-text defects (we would not "fix" the wording, so skipped)
- **Квітка-Основʼяненко, «Маруся», «Сватання на Гончарівці», «Конотопська відьма»**: ukrlib's text uses Latin «i» in place of Cyrillic «і» throughout, which fails the homoglyph guard. Candidates lost include «Не можу iз собою зовладати, бо любов, як сон: нi заїси, нi запʼєш…» and «Нема на свiтi луччого щастя, як з дурнем жити!». Quotes were taken from «Сердешна Оксана» instead, which has clean text.
- **Нечуй-Левицький, «Кайдашева сімʼя»**: «Така нудьга мене бере, що… i зелені луги сльозами залила.» The passage contains Latin «i».
- **Франко, «Ой ти, дівчино, з горіха зерня…»** (ukrlib tid 622) begins with a Latin «O». We used the same poem from the collection text «Зівʼяле листя» (tid 3422) instead.
- **Кобилянська, «Земля»**: «Неначе зіштивніле [13] море, простерлася земля…». An editorial footnote marker sits inside the sentence.
- **Кобилянська, «Людина»**: «Ніяка сила світу не стопче в мені мислячої самостійної людини,. …». The source has a typo (",.").
- **П. Куліш, «Рідне слово»**, first stanza: «3 вітром розмовляють» has the digit 3 where the letter З should be (an OCR error). We used a different stanza of the poem.
- **Олесь, «Чари ночі»**: «Сміються, плачуть солов'ї… "Цілуй, цілуй, цілуй її, — / Знов молодість не буде!». The quotation mark stays open in the source, so the passage could not be closed cleanly.
- **Шевченко, «Кобзар»** (ukrlib tid 3545, a whole-book file): it has OCR slips such as «запглили» and «Заміс[т]ь». We used the single-poem files instead.

## Cut to keep quality or length (verified, but weaker or redundant)
These eight all passed verification and were dropped only for quality or redundancy.
- Грінченко «Каторжна»: «Минула весна, літо минає... Чи не мина з ними й щастя?»
- Косинка «В житах»: «Це все було просто до дрібниць: і я, і заспаний ранок, і сивий степ.»
- Стефаник «Камінний хрест»: «Глядить із берега на воду, як на утрачене щастя.» Unclear without context.
- Кобилянська «Царівна»: «Я рада би приглянутися кождій речі до дна…». Needed a trailing cut.
- Марко Вовчок «Козачка»: «Було тільки сонця краєчок засвітить, уже й бряк, і дзвяк по селу…»
- Плужник «Все більше спогадів і менше сподівань…». Too funereal for a sleep screen.
- Коцюбинський «Intermezzo»: «І благословен я був між золотим сонцем й зеленою землею.» Redundant with other Intermezzo lines.
- Франко «Земле, моя всеплодющая мати…», first stanza. We kept the «Дай працювать…» lines from the same poem.

## Considered, not usable for other reasons
- **Too long or would need a mid-thought cut**: Шевченко «Минають дні…». We kept a 4-line cut ending «…» and dropped the rest of the sentence. Кобилянська «Valse mélancolique» (several sentences over 220 characters). Коцюбинський «Intermezzo», the «Я п'ю тебе, сонце…» paragraph.
- **Russian-language works**: Старицький «Богдан Хмельницький» (trilogy on ukrlib, in Russian) and Марко Вовчок's Russian stories. Excluded.
- **Translations**: none used. Сковорода was excluded because modern Ukrainian renderings are translations. For Зеров, only his original poems were used, none of his translations.
- **Not public domain in Ukraine (author died in 1954 or later)**: Остап Вишня (d. 1956), Максим Рильський (1964), Павло Тичина (1967), Євген Маланюк (1968), Володимир Сосюра (1965), Микола Бажан (1983). These are candidates for the personal set only.

## Re-verification of «бог»/«Бог» capitalisation (2026-09-23)
Two entries lowercased «бог» because their source (ukrlib.com.ua, which itself follows a Soviet-era-style digitisation) does. Both were checked against a modern (post-1991) academic edition to see if the entry could simply be re-capitalised. Neither could — the modern edition's wording differs from ours by more than the letter case, so per the verbatim-match rule both were removed rather than "fixed" with a mixed source. No replacement quotes were added.

- **Шевченко, «Кавказ»** (was id 102): our text — «Борітеся — поборете!\nВам бог помагає!\nЗа вас правда, за вас слава\nІ воля святая!» — matches ukrlib.com.ua verbatim (checked directly: https://www.ukrlib.com.ua/books/printit.php?tid=741). The modern critical edition — Шевченко Т. Г. Повне зібрання творів: У 12 т. / Редкол.: М. Г. Жулинський (голова) та ін. — К.: Наук. думка, 2001. Т. 1: Поезія 1837–1847 (ISBN 966-00-0712-4), as digitised at http://litopys.org.ua/shevchenko/shev139.htm (mirrored at izbornyk.org.ua) — does capitalise «Бог» («Вам Бог помагає!»), confirming the capitalisation fix is correct. But that edition also punctuates the previous line with a comma, not an exclamation mark: «Борітеся — поборете,» vs. our «Борітеся — поборете!». That's a real wording/punctuation difference, not covered by the apostrophe/dash/whitespace normalisation, so the quote is not a verbatim match once you also fix the capital letter — it would require silently changing punctuation too. Removed rather than mixed. (For reference, several government/university sites — e.g. tsdahou.archives.gov.ua, um.kpnu.edu.ua — quote the line with both the capital «Бог» and the exclamation mark, but as prose epigraphs with no cited edition and no preserved verse lineation, they don't qualify as a reputable primary-text source to confirm against.)
- **Стефаник, «Сини»** (was id 75): our text — «А крізь сонце бог, як крізь золоте сито, обсипав нас ясностев, і вся земля, і всі люди відблискували золотом.» — matches ukrlib.com.ua verbatim (https://www.ukrlib.com.ua/books/printit.php?tid=341), and is in fact identical (lowercase «бог», both commas, «відблискували») to the 1949 Soviet «Повне зібрання творів» printing as reproduced at https://zbruc.eu/node/36992 (citing «ПЗТ, I, 203»). The earliest lifetime edition — «Земля». Нариси й оповідання (1926), с. 35–44, as reproduced at https://zbruc.eu/node/36991 — does capitalise «Бог», and the same capitalisation is preserved in the 1942 «Твори» reprint on Ukrainian Wikisource (https://uk.wikisource.org/wiki/Твори_(Стефаник,_1942)/Сини). But both of those pre-1949 printings also drop the two commas («…ясностев і вся земля і всі люди…») and spell the last word «віблискували» (missing «д»), so neither is a verbatim match to our text beyond the capital letter either. No post-1991 edition combining the capital «Бог» with our exact wording/punctuation could be found (checked ukrlit.org, md-eksperiment.org, chtyvo.org.ua [site now closed], and searched for post-1991 print editions of «Сини» in school anthologies and collected-works volumes — all either reproduce the lowercase 1949-style text or weren't locatable in full text). Removed rather than mixed.

## Related
- `quotes.json`: the public-domain set
- `verification.md`: the per-quote PASS log
