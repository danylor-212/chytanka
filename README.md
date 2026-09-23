<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/chytanka/assets/logo-dark.png">
  <img src="docs/chytanka/assets/logo.png" width="96" alt="Логотип Читанки: тризуб на обкладинці книжки">
</picture>

# Читанка

**Українська прошивка для електронних читалок Xteink X3 і X4**<br>
на основі [CrossInk](https://github.com/uxjulia/CrossInk) · [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader)

<img src="docs/chytanka/assets/hero.png" alt="Читанка на Xteink X4 і X3" width="820">

[Що всередині](#що-всередині) · [Екрани](#екрани) · [Цитати](#цитати-на-заставці) · [Безкоштовні книжки](#безкоштовні-книжки) · [Встановлення](#встановлення) · [Збірка](#збірка-з-вихідного-коду) · [English](#english)

</div>

---

> [!NOTE]
> **Статус: підготовка першого релізу.** Прошивка збирається й проходить автоматичні тести, але ще не пройшла повну перевірку на пристрої. Перш ніж встановлювати, зробіть бекап (див. [Встановлення](#встановлення)).

## Що всередині

Читанка — це CrossInk, доведений до ладу для українського читача. Усе, за що люблять CrossInk (панель зі статистикою читання, швидкий рушій EPUB, синхронізація з KOReader), лишається. Поверх додано:

- 🇺🇦 **Інтерфейс повністю українською.** Перекладено всі 922 рядки, без англійських «дірок». Одиниці часу (`12 год 55 хв`), десяткова кома, назви місяців. Довгі слова не вилазять за межі кнопок і колонок.
- ✂️ **Правильні українські переноси.** Виправлено переноси слів з великої літери (`Є І Ї Ґ`, зокрема в заголовках КАПСОМ) і слів з апострофом `ʼ` («обʼєднання», «під’-їзд» з дефісом за правописом).
- 📜 **Цитати української літератури на заставці.** 50 карток уже вбудовано в прошивку, повний набір зі 109 можна покласти на SD-картку. Без повторів, темний режим інвертує картку.
- 📖 **65 безкоштовних книжок української класики** в каталозі OPDS [«Читанка — Книжки»](https://danylor-212.github.io/chytanka-books/): Франко, Леся Українка, Коцюбинський, Хвильовий, Підмогильний, Котляревський та ще три десятки авторів. Завантажуються прямо з читалки через Wi‑Fi.
- 📚 **Бренд Читанки.** Логотип на екрані завантаження, назва пристрою `Chytanka X4 CrossInk`.
- 🔄 **Власний канал оновлень.** OTA шукає релізи Читанки, тож оновлення CrossInk не перезапише українську збірку.
- ⚡ **Легша збірка.** Вшито лише дві мови інтерфейсу (українська й англійська), тож прошивка менша й лишає більше місця.

Підтримувані пристрої: **Xteink X4** і **Xteink X3** (ESP32-C3). X4 Pro, X4 Classic і Sticky поки не підтримуються.

## Екрани

<div align="center">
<img src="docs/chytanka/assets/gallery.png" alt="Читалка, налаштування, статистика та бібліотека українською" width="900">
</div>

Зліва направо: сторінка книжки з українськими переносами, налаштування, статистика читання, бібліотека. Усе це справжній вивід прошивки із симулятора CrossInk; статистика на скрінах демонстраційна.

<div align="center">
<img src="docs/chytanka/assets/boot-sleep.png" alt="Екран завантаження та заставки: світла й темна тема" width="700">
</div>

Екран завантаження з логотипом Читанки та заставка з цитатою у світлій і темній темі.

<details>
<summary><b>Xteink X3</b>: ті самі екрани на меншому пристрої</summary>
<br>
<div align="center">
<img src="docs/chytanka/assets/x3-gallery.png" alt="Читанка на Xteink X3: завантаження, панель, статистика, заставка" width="900">
</div>
</details>

## Цитати на заставці

<div align="center">
<img src="docs/chytanka/assets/quotes.png" alt="Цитати Лесі Українки, Антонича, Котляревського та Підмогильного" width="900">
</div>

Щоразу, коли читалка засинає, на екрані з'являється нова цитата: Шевченко, Франко, Леся Українка, Коцюбинський, Стефаник, Кобилянська, Хвильовий, Антонич, Плужник, Олесь, Теліга та ще два десятки авторів.

- **50 цитат уже вбудовано в прошивку.** Кожна картка стиснута до ~6 КБ і розпаковується рядок за рядком, тож у пам'яті ніколи не лежить повне зображення. Показ іде «мішечком»: кожна картка з'являється раз за коло.
- **Повний набір зі 109 карток** можна покласти в теку `/.sleep/` на SD-картці й вибрати заставку *Власний* (*Налаштування → Екран → Заставка → Шпалери*).
- **Кожна цитата звірена дослівно** з повним текстом твору. Усі автори — у суспільному надбанні (померли до 1954 року). Перелік джерел і відхилених «інтернет-цитат» — у [`docs/chytanka/quotes.md`](docs/chytanka/quotes.md).
- **«Цитати — темна тема»** показує картку інвертованою: світлий текст на чорному, у 4 відтінках сірого.

<div align="center">
<img src="docs/chytanka/assets/x3-quote.png" alt="Цитата Шевченка на Xteink X3" width="260">
</div>

## Безкоштовні книжки

<div align="center">
<img src="docs/chytanka/assets/opds.png" alt="Каталог «Читанка — Книжки» на читалці: список книжок і обкладинка «Лісової пісні»" width="620">
</div>

**[«Читанка — Книжки»](https://danylor-212.github.io/chytanka-books/)** — відкритий каталог української класики в суспільному надбанні: **65 книжок** прози, поезії, драми, дитячої літератури та спогадів. «Енеїда», «Лісова пісня», «Тіні забутих предків», «Захар Беркут», «Чорна рада», «Місто», «Сині етюди», «Камена», «Мартин Боруля» та інші.

- **Прямо з читалки.** Каталог працює за стандартом OPDS: *Головна → Меню → Каталог OPDS → «Читанка — Книжки»*, вибираєте книжку, і вона завантажується по Wi‑Fi в бібліотеку. Починаючи з версії 1.6.0.1 каталог уже вбудований у прошивку.
- **Адреса OPDS:** `https://danylor-212.github.io/chytanka-books/opds/index.xml` — підходить і для інших читалок та застосунків з OPDS (KOReader, PocketBook, Moon+ Reader, Thorium).
- **Якісні EPUB.** Тексти — з вичитаних за сканами видань на [uk.wikisource.org](https://uk.wikisource.org), здебільшого 1920‑х років, з правописом, близьким до сучасного. Кожна книжка має обкладинку в стилі Читанки, сторінку «Про це видання» з джерелом і проходить перевірку [EPUBCheck](https://github.com/w3c/epubcheck).
- **Легкі файли.** Зайві вбудовані шрифти вирізано: книжки важать 40 КБ – 1 МБ замість 4–5 МБ.
- **Чесно щодо прав.** Усі автори померли до 1954 року. Редакторські примітки й ілюстрації, права на які не підтверджені, прибрано; шар Вікіджерел — за CC BY-SA 4.0.
- **Оновлюється сам.** Каталог перебудовується щотижня з Вікіджерел, тож виправлення волонтерів підтягуються автоматично. Код і список книжок — у [danylor-212/chytanka-books](https://github.com/danylor-212/chytanka-books); пропозиції нових творів вітаються.

## Встановлення

> [!WARNING]
> Прошивка через USB перезаписує всю пам'ять пристрою. Спершу зробіть повний бекап, тоді повернутися до попередньої прошивки можна однією командою.

1. **Бекап** (приблизно 2 хвилини, пристрій підключений USB-кабелем):
   ```bash
   pipx install esptool
   esptool --chip esp32c3 --port /dev/ttyACM0 read-flash 0 ALL x4-backup.bin
   ```
2. **Прошивка.** Файл `firmware-x3-x4.bin` беріть зі сторінки [релізів](../../releases), або зберіть самі (див. нижче) і прошийте через `pio run -e ua -t upload`.
3. **Мова.** Якщо на пристрої раніше стояв CrossInk або CrossPoint, він пам'ятає збережену мову. Перемкніть один раз: *Settings → Language → Українська*. На новому пристрої українська вмикається сама.
4. **Цитати на SD (за бажанням).** Скопіюйте картки в `/.sleep/` і виберіть заставку *Власний*.

**Відкат:** `esptool --chip esp32c3 --port /dev/ttyACM0 write-flash 0 x4-backup.bin`

**Перехід з CrossInk:** налаштування, статистика читання й бібліотека зберігаються. Кеш книжок перебудується, тож кожна книжка першого разу відкриється трохи повільніше.

## Збірка з вихідного коду

Потрібен [PlatformIO Core від pioarduino](https://github.com/pioarduino/platformio-core) **v6.1.19**, як у CI. Шлях до проєкту має бути без пробілів (цього вимагає ESP-IDF).

```bash
git clone --recursive https://github.com/danylor-212/chytanka.git
cd chytanka
pio run -e ua                 # Читанка для X3/X4
pio run -e ua -t upload       # прошити підключений пристрій
```

Правила оновлення від CrossInk і випуску релізів описано в [`docs/chytanka/RELEASING.md`](docs/chytanka/RELEASING.md).

## Подяки

- **[CrossInk](https://github.com/uxjulia/CrossInk)** від Julia Nguyen: основа прошивки, панель і статистика читання. Оригінальний README — у [`docs/chytanka/CROSSINK_README.md`](docs/chytanka/CROSSINK_README.md).
- **[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)**: рушій читалки, від якого походить CrossInk. Виправлення українських переносів готуємо як внесок у CrossPoint, щоб їх отримали всі.
- Шрифт карток **[Literata](https://github.com/googlefonts/literata)** (SIL Open Font License).
- Тексти цитат: [uk.wikisource.org](https://uk.wikisource.org), [ukrlib.com.ua](https://www.ukrlib.com.ua), академічне ПЗТ Шевченка ([litopys.org.ua](http://litopys.org.ua)).

Ліцензія: **MIT**, як у CrossInk і CrossPoint.

---

## English

**Chytanka** («Читанка», *a reader*) is a Ukrainian edition of [CrossInk](https://github.com/uxjulia/CrossInk) firmware for the Xteink X3 and X4 e-readers. It keeps everything CrossInk does (the dashboard with reading stats, the fast EPUB engine, KOReader sync) and adds:

- **A complete Ukrainian UI.** All 922 strings are translated, with localised durations, decimal comma and dates, and layout fixes so longer words fit.
- **Correct Ukrainian hyphenation:** capital `Є І Ї Ґ` letters and the `ʼ` apostrophe (being prepared as an upstream contribution to CrossPoint).
- **Ukrainian literature on the sleep screen.** 50 compressed cards are built into the firmware, and the full set of 109 can go on the SD card. Every quote was checked word for word against its source text, and all authors are public domain.
- **65 free public-domain Ukrainian classics** in the [«Читанка — Книжки»](https://danylor-212.github.io/chytanka-books/) OPDS catalogue (`https://danylor-212.github.io/chytanka-books/opds/index.xml`), downloadable over Wi‑Fi straight from the reader and usable in any OPDS app.
- **Chytanka branding and its own OTA channel**, so a CrossInk update can't replace it.
- **UK + EN built-in** for a smaller image.

Status: preparing the first release; not yet fully tested on hardware. Back up your device before flashing. MIT licensed.
