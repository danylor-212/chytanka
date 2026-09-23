"""Run with python3 -m unittest discover -s test/scripts -p 'test_gen_i18n.py'."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import gen_i18n  # noqa: E402

SENTINEL = gen_i18n.ENGLISH_FALLBACK_OFFSET


def string_of_blob_size(size: int) -> str:
    """A string that takes `size` bytes in the blob (text plus its NUL)."""
    return "x" * (size - 1)


class OffsetTableTest(unittest.TestCase):
    def offsets_for(self, first_size: int):
        english = ["a", "b"]
        # The first string fills the blob so the second starts at first_size.
        return gen_i18n.build_offset_table("XX", [string_of_blob_size(first_size), "y"], english)

    def test_start_below_sentinel_is_kept(self):
        offsets, blob = self.offsets_for(0xFFFE)
        self.assertEqual(offsets, [0, 0xFFFE])
        self.assertEqual(len(blob), 2)

    def test_start_at_sentinel_is_rejected(self):
        # A string starting at 0xFFFF would read back as "use English".
        with self.assertRaisesRegex(ValueError, "start offsets must stay below 65535"):
            self.offsets_for(0xFFFF)

    def test_start_past_sentinel_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "would start at byte offset 65536"):
            self.offsets_for(0x10000)

    def test_english_copies_use_the_sentinel(self):
        offsets, blob = gen_i18n.build_offset_table("XX", ["a", "own"], ["a", "b"])
        self.assertEqual(offsets, [SENTINEL, 0])
        self.assertEqual(blob, ["own"])


class FormatCheckTest(unittest.TestCase):
    def check(self, english: str, translated: str):
        return gen_i18n.check_format_specifiers(["EN", "XX"], ["STR_K"], {"STR_K": [english, translated]})

    def test_matching_conversions_pass(self):
        self.assertEqual(self.check("Page %d, %.2f%% overall", "Seite %d, %.2f %% gesamt"), ([], []))

    def test_width_flags_and_d_vs_i_are_equivalent(self):
        self.assertEqual(self.check("%s: %d", "%-5s: %i"), ([], []))

    def test_type_change_is_an_error(self):
        errors, _ = self.check("%d items", "%s items")
        self.assertEqual(len(errors), 1)

    def test_stray_space_flag_conversion_is_an_error(self):
        errors, _ = self.check("Page %d, %.2f%% overall", "Seite %d, %.2f %%% Gesamt")
        self.assertEqual(len(errors), 1)

    def test_extra_star_argument_is_an_error(self):
        errors, _ = self.check("%.2f", "%.*f")
        self.assertEqual(len(errors), 1)

    def test_positional_conversions_are_rejected(self):
        errors, _ = self.check("%s of %s", "%2$s von %1$s")
        self.assertIn("positional", errors[0])

    def test_dropping_trailing_conversions_warns(self):
        errors, warnings = self.check("Page %d/%d, %.2f%% overall", "Page %d/%d")
        self.assertEqual(errors, [])
        self.assertEqual(len(warnings), 1)

    def test_conversions_added_to_plain_text_warn(self):
        errors, warnings = self.check("Books read", "Прочитано %d")
        self.assertEqual(errors, [])
        self.assertEqual(len(warnings), 1)

    def test_plain_percent_text_is_not_a_format(self):
        self.assertEqual(self.check("You have reached 99% of this book", "Ви прочитали 99% цієї книжки"), ([], []))

    def test_date_patterns_are_skipped(self):
        result = gen_i18n.check_format_specifiers(
            ["EN", "XX"], ["STR_DATE_PATTERN"], {"STR_DATE_PATTERN": ["{m} {dd}, {y}", "{d} {m} %s"]}
        )
        self.assertEqual(result, ([], []))


if __name__ == "__main__":
    unittest.main()
