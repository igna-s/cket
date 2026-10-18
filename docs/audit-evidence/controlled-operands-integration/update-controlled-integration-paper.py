"""Bind paper checks to the138-input integration and preserve137 history."""


def update(root):
    path = root / 'docs/paper/audit_paper.py'
    text = path.read_text()
    start = text.index('    current_integration = json.loads(')
    end = text.index('    coherent_audit = json.loads(', start)
    text = text[:start] + '''    previous_integration = json.loads(
        (ROOT / 'docs/audit-evidence/coherent-memory-integration/integration-summary.json').read_text()
    )
    current_integration = json.loads(
        (ROOT / 'docs/audit-evidence/controlled-operands-integration/integration-summary.json').read_text()
    )
    current_full = json.loads((ROOT / 'docs/audit-evidence/controlled-operands-full/summary.json').read_text())
    previous_full = json.loads((ROOT / 'docs/audit-evidence/coherent-control-full/summary.json').read_text())
    check(
        'Current compiler:138 integrated inputs match targeted installation and completed broad sources',
        current_integration['root_verification']['status'] == 'passed-targeted'
        and current_integration['source_files'] == len(current_integration['source_sha256']) == 138
        and current_integration['source_sha256'] == current_full['source_sha256']
        and current_full['status'] == 'verified-broad-campaign' and current_full['exit_code'] == 0
        and current_full['closed_commands'] == 76
        and all(hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == digest
                for name, digest in current_integration['source_sha256'].items())
        and all(row['native_checks'] == 393 and row['controlled_operand_checks'] == 1270
                and row['exact_contract_executions'] == 40
                for row in current_integration['root_verification']['configurations']),
    )
    check(
        'Historical137 broad campaign completed separately from its original targeted integration metadata',
        previous_full['status'] == 'verified-broad-campaign' and previous_full['exit_code'] == 0
        and previous_full['closed_commands'] == 74
        and previous_full['source_sha256'] == previous_integration['source_sha256']
        and previous_integration['source_files'] == 137
        and previous_integration['source_sha256'] != current_integration['source_sha256'],
    )
''' + text[end:]
    old = "for row in current_integration['root_verification']['configurations']\n        )\n        and 'corrección integrada'"
    assert text.count(old) == 1
    text = text.replace(old, "for row in previous_integration['root_verification']['configurations']\n        )\n        and 'corrección integrada'")
    old = "and coherent_maintained['source_sha256'] == current_integration['source_sha256']"
    assert text.count(old) == 1
    text = text.replace(old, "and coherent_maintained['source_sha256'] == previous_integration['source_sha256']")
    text = text.replace('Current audit: seven controlled-body failures remain disclosed despite the condition fix',
        'Historical137 audit: seven original controlled-body failures retain their source identity')
    text = text.replace('Candidate operands: two actual broad failures prevent promotion',
        'Historical v4 operands: two actual broad failures prevented promotion of those sources')
    old = "        and 'dos regresiones' in re.sub(r'\\s+', ' ', overview)\n        and 'no está integrada' in re.sub(r'\\s+', ' ', overview),"
    assert text.count(old) == 1
    text = text.replace(old, "        and controlled_failure['source_sha256'] != current_integration['source_sha256'],")
    old = "and operand_v5['source_sha256'] != current_integration['source_sha256']"
    assert text.count(old) == 1
    text = text.replace(old, "and operand_v5['source_sha256'] == current_integration['source_sha256']")
    text = text.replace('V5 candidate: pure CPU loads still fault despite valid ordinary C inactive arms',
        'Integrated v5: pure CPU loads still fault despite valid ordinary C inactive arms')
    old = "and integrated_ci['source_sha256'] == current_integration['source_sha256']"
    assert text.count(old) == 1
    text = text.replace(old, "and integrated_ci['source_sha256'] == previous_integration['source_sha256']")
    marker = "                'docs/c11-memory-sequences.md',\n"
    assert text.count(marker) == 1
    text = text.replace(marker, marker + "                'docs/c11-controlled-operands.md',\n                'docs/re-audit-2026-10-18-controlled-operands.md',\n")
    marker = "                'integrated137-remote-ci',\n"
    assert text.count(marker) == 1
    text = text.replace(marker, marker + "                'coherent-control-full',\n                'controlled-operands-full',\n                'controlled-operands-integration',\n")
    path.write_text(text)
