"""Regressions for the original false 8 > 12 claim in both editions."""

import unittest
from paper_claims import enumeration_claim


class EnumerationClaims(unittest.TestCase):
    def test_original_spanish_is_false(self):
        with self.assertRaises(ValueError):
            enumeration_claim(
                'Con 8, 16, 24 y 32 bits relevantes, las cargas superan el límite de enumeración de 12 bits. --no-tables',
                'es',
                {8, 16, 24, 32},
                12,
            )

    def test_original_english_is_false(self):
        with self.assertRaises(ValueError):
            enumeration_claim(
                'With 8, 16, 24, and 32 relevant bits, the workloads exceed the 12-bit enumeration limit. --no-tables',
                'en',
                {8, 16, 24, 32},
                12,
            )

    def test_correct_claims_match_the_measured_widths(self):
        for language, text in (
            (
                'es',
                'Las cargas con 16, 24 y 32 bits relevantes superan el límite de enumeración de 12 bits. --no-tables',
            ),
            (
                'en',
                'Workloads with 16, 24, and 32 relevant bits exceed the 12-bit enumeration limit. --no-tables',
            ),
        ):
            result = enumeration_claim(text, language, {8, 16, 24, 32}, 12)
            self.assertEqual(result['not_above_limit'], [8])

    def test_equal_to_limit_is_not_above(self):
        result = enumeration_claim(
            'Workloads with 16 relevant bits exceed the 12-bit enumeration limit. --no-tables',
            'en',
            {8, 12, 16},
            12,
        )
        self.assertEqual(result['not_above_limit'], [8, 12])


if __name__ == '__main__':
    unittest.main()
