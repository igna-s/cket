"""Small semantic checks for quantitative claims in the bilingual papers."""

from __future__ import annotations

import re


def enumeration_claim(source: str, language: str, relevant_bits: set[int], limit: int) -> dict:
    patterns = {
        'es': r'(?:Con|cargas con) ([\d, y]+) bits relevantes[^.]*?superan[^.]*?enumeraci[oó]n de (\d+) bits',
        'en': r'(?:With|Workloads with) ([\d, and]+) relevant bits[^.]*?exceed[^.]*?(\d+)-bit enumeration limit',
    }
    matches = re.findall(patterns[language], source)
    if len(matches) != 1:
        raise ValueError('The manuscript needs one unambiguous enumeration-threshold claim')
    numbers, claimed_limit = matches[0]
    claimed = [int(n) for n in re.findall(r'\d+', numbers)]
    expected = sorted(bits for bits in relevant_bits if bits > limit)
    if int(claimed_limit) != limit or sorted(claimed) != expected:
        raise ValueError(
            f'Enumeration claim {claimed} > {claimed_limit}; measured widths require {expected} > {limit}'
        )
    if '--no-tables' not in source:
        raise ValueError('The selected table-free policy must be explicit')
    return dict(
        relevant_bits=sorted(relevant_bits),
        limit=limit,
        above_limit=expected,
        not_above_limit=sorted(relevant_bits - set(expected)),
        table_policy='--no-tables',
    )
