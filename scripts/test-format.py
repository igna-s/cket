#!/usr/bin/env python3
"""Regression checks for formatter-induced loss of language contracts."""
import importlib
import unittest

formatter = importlib.import_module('format')


class MarkdownContracts(unittest.TestCase):
    def test_unescaped_operator_cannot_drop_a_contract(self):
        text = '| Class | Restriction | Work |\n| --- | --- | --- |\n| P | Effects in `?:`, `&&` and `||` | Schedule selected effects and retain history |\n'
        with self.assertRaisesRegex(ValueError, 'Markdown content loss'):
            formatter.format_text(formatter.ROOT / 'docs/contract.md', text)

    def test_escaped_operator_and_work_survive(self):
        text = '| Class | Restriction | Work |\n| --- | --- | --- |\n| P | Effects in `?:`, `&&` and `\\|\\|` | Schedule selected effects and retain history |\n'
        result = formatter.format_text(formatter.ROOT / 'docs/contract.md', text)
        self.assertIn('Schedule selected effects and retain history', result)
        self.assertIn('\\|\\|', result)

    def test_numbered_list_reformatting_keeps_the_steps(self):
        text = '1. First step.\n2. Second step.\n'
        result = formatter.format_text(formatter.ROOT / 'docs/contract.md', text)
        self.assertIn('First step.', result)
        self.assertIn('Second step.', result)


if __name__ == '__main__':
    unittest.main()
