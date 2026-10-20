#!/usr/bin/env python3
"""Verify document evidence, examples, references, PDF text and geometry."""
from __future__ import annotations

import csv
import hashlib
import importlib.metadata
import json
import math
import re
import subprocess
import tarfile
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

import pymupdf as fitz
from PIL import Image
from pypdf import PdfReader

BASE = Path(__file__).resolve().parent
ROOT = BASE.parent.parent
EVIDENCE = BASE / "evidence"
PDFS = {
    "es": ROOT / "output" / "pdf" / "c_ket_paper.pdf",
    "en": ROOT / "output" / "pdf" / "c_ket_paper_en.pdf",
}
SOURCES = {"es": BASE / "c_ket_paper.md", "en": BASE / "c_ket_paper_en.md"}
TMP = ROOT / "tmp" / "pdfs"
checks = []
INTERNAL_EDITORIAL = re.compile(
    r'docs/paper/|\b(?:docs|build)/[\w./-]+|synthesis-growth\.csv|\b(?:checkout|commit)\b|'
    r'\*\*(?:Disponibilidad|Availability)\.\*\*|MIT-licensed|código MIT|'
    r'consultad[oa]s? localmente|consulted locally|metas abiertas|open goals',
    re.I,
)


def check(name, condition, detail=""):
    checks.append({"check": name, "passed": bool(condition), "detail": detail})
    if not condition:
        raise AssertionError(f"{name}: {detail}")


def run(command):
    result = subprocess.run(
        [str(c) for c in command], cwd=ROOT, capture_output=True, text=True, timeout=120
    )
    check(
        "Ejemplo: " + " ".join(str(c) for c in command[1:3]),
        result.returncode == 0,
        result.stderr[-1000:] if result.returncode else "Código de salida 0",
    )
    return result.stdout


def write_json(path, value, *, ensure_ascii=True):
    # Write atomically so a failed/truncated report never replaces valid
    # historical evidence. This also avoids in-place DrvFS sharing conflicts.
    with tempfile.NamedTemporaryFile(
        mode='w',
        encoding='utf-8',
        dir=path.parent,
        prefix=path.name + '.',
        suffix='.tmp',
        delete=False,
    ) as stream:
        temporary = Path(stream.name)
        stream.write(json.dumps(value, ensure_ascii=ensure_ascii, indent=2) + '\n')
    try:
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def main(language):
    checks.clear()
    tmp = TMP / language
    tmp.mkdir(parents=True, exist_ok=True)
    source = SOURCES[language].read_text(encoding="utf-8")
    references_heading = '## Referencias' if language == 'es' else '## References'
    figure_word, table_word = ('Figura', 'Tabla') if language == 'es' else ('Figure', 'Table')
    section_titles = (
        [
            "1 Introducción",
            "2 Antecedentes",
            "3 Arquitectura del sistema",
            "4 Motivación y costos del modelo",
            "5 Configuración experimental",
            "6 Resultados",
            "7 Conclusiones",
        ]
        if language == 'es'
        else [
            "1 Introduction",
            "2 Background",
            "3 System Architecture",
            "4 Motivation and Model Costs",
            "5 Experimental Setup",
            "6 Results",
            "7 Conclusions",
        ]
    )
    check(
        "Siete secciones con la progresión del modelo",
        re.findall(r'^## (\d .+)$', source, re.M) == section_titles,
    )
    check(
        "Conclusiones y bibliografía comienzan en la página final",
        source.count('<!-- page -->') == 1 and '\n<!-- page -->\n\n## 7 ' in source,
    )
    spanish = SOURCES['es'].read_text(encoding='utf-8')
    english = SOURCES['en'].read_text(encoding='utf-8')
    check(
        "Mismos bloques C y JSON en ambos idiomas",
        re.findall(r'```(\w+)\n(.*?)```', spanish, re.S)
        == re.findall(r'```(\w+)\n(.*?)```', english, re.S),
    )
    check("Manuscrito sin notas internas de edición", not INTERNAL_EDITORIAL.search(source))
    check(
        "Manuscrito sin instrucciones de mantenimiento",
        all(kind in ('c', 'json') for kind in re.findall(r'```(\w+)', source)),
    )
    check(
        "Mismas ecuaciones en ambos idiomas",
        re.findall(r'\$\$\n(.*?)\n\$\$', spanish, re.S)
        == re.findall(r'\$\$\n(.*?)\n\$\$', english, re.S),
    )
    check(
        "Mismos enlaces bibliográficos en ambos idiomas",
        re.findall(r'\]\((https?://[^)]+)\)', spanish)
        == re.findall(r'\]\((https?://[^)]+)\)', english),
    )
    spanish_numbers = re.findall(r'\b\d+(?:[.,]\d+)*\b', spanish.split('## Referencias')[0])
    english_numbers = re.findall(r'\b\d+(?:[.,]\d+)*\b', english.split('## References')[0])
    check(
        "Mismas cifras y contratos cuantitativos en ambos idiomas",
        [n.replace(',', '.') for n in spanish_numbers]
        == [n.replace(',', '.') for n in english_numbers],
        f"{len(spanish_numbers)} cifras ES; {len(english_numbers)} cifras EN",
    )
    logs = {
        "native.log": [r"354 passed, 0 failed", r"426 checks passed across 16 examples"],
        "frontend.log": [r"145 generated frontend compilations passed; seed=20261011"],
        "growth.log": [r"24 circuits compiled without tables"],
        "qasm.log": [r"11 exported circuits independently validated with QuEST 3.7.0"],
    }
    for name, patterns in logs.items():
        content = (EVIDENCE / name).read_text()
        for pattern in patterns:
            check("Resultado reproducido: " + pattern, bool(re.search(pattern, content)))
        check("Ausencia de FAIL en " + name, not re.search(r'^FAIL\b', content, re.M))
    environment = (EVIDENCE / "environment.txt").read_text()
    check("Commit de referencia", "1ebb2e308e61bed5632798ded5beecf77b6b9a9c" in environment)
    check("GCC de referencia", "11.4.0" in environment)
    check("Build del proyecto", "Built |c>" in (EVIDENCE / "build.log").read_text())

    with (EVIDENCE / "synthesis-growth.csv").open() as stream:
        data = list(csv.DictReader(stream))
    check("24 filas de síntesis", len(data) == 24)
    check(
        "Diseño completo de 6 operadores por 4 anchos",
        {(r["operator"], int(r["width"])) for r in data}
        == {(op, w) for op in ("add", "sub", "mul", "div", "mod", "lt") for w in (4, 8, 12, 16)},
    )
    labels = (
        {
            "Suma": "add",
            "Resta": "sub",
            "Multiplicación": "mul",
            "División": "div",
            "Resto": "mod",
            "Menor que": "lt",
        }
        if language == 'es'
        else {
            "Addition": "add",
            "Subtraction": "sub",
            "Multiplication": "mul",
            "Division": "div",
            "Remainder": "mod",
            "Less than": "lt",
        }
    )
    for label, op in labels.items():
        matches = re.findall(r'^\| ' + re.escape(label) + r' \|(.+)\|$', source, re.M)
        check("Dos tablas con operador " + label, len(matches) == 2)
        gates = [int(x.strip()) for x in matches[0].split('|')]
        expected = [
            int(next(r for r in data if r['operator'] == op and int(r['width']) == w)['gates'])
            for w in (4, 8, 12, 16)
        ]
        check("Puertas de " + label, gates == expected)
        actual = [int(x.strip()) for x in matches[1].split('|')]
        row = next(r for r in data if r['operator'] == op and r['width'] == '16')
        check(
            "Workspace y profundidad de " + label,
            actual == [int(row[k]) for k in ('gates', 'ancillas', 'peak_qubits', 'depth')],
        )
    for r in data:
        check(
            "Bits relevantes " + r['operator'] + ' ' + r['width'],
            int(r['relevant_bits']) == 2 * int(r['width']),
        )

    refs = {int(n) for n in re.findall(r'^\[(\d+)\]', source, re.M)}
    body = source.split(references_heading)[0]
    cited = {
        int(n) for group in re.findall(r'\[(\d+(?:,\s*\d+)*)\]', body) for n in group.split(',')
    }
    check("Diez referencias identificadas y citadas", refs == set(range(1, 11)) and cited == refs)
    figures = re.findall(r'!\[(' + figure_word + r' (\d+)\..+?)\]\((.+?)\)', source)
    check("Siete figuras consecutivas", [int(x[1]) for x in figures] == list(range(1, 8)))
    for caption, number, path in figures:
        target = BASE / path
        check(
            "Figura " + number + " disponible",
            target.is_file() and target.with_suffix('.svg').is_file(),
        )
        image = Image.open(target)
        check(
            "Resolución figura " + number,
            image.width >= 1500 and image.height >= 450,
            f"{image.width} x {image.height} píxeles",
        )
        svg_text = ' '.join(ET.parse(target.with_suffix('.svg')).getroot().itertext())
        markers = (
            {
                '1': 'Vector de estado',
                '2': 'fase',
                '3': 'Probabilidad conjunta',
                '4': 'Iteraciones de Grover',
                '5': 'Memoria del vector',
                '6': 'Puertas lógicas',
                '7': 'Pico de qubits',
            }
            if language == 'es'
            else {
                '1': 'State vector',
                '2': 'phase',
                '3': 'Joint probability',
                '4': 'Grover iterations',
                '5': 'Vector memory',
                '6': 'Logical gates',
                '7': 'Peak represented qubits',
            }
        )
        check("Figura " + number + " con etiquetas en " + language, markers[number] in svg_text)
    check(
        "Sin placeholders",
        not re.search(r'\b(?:TODO|TBD|FIXME)\b', source) and 'lorem ipsum' not in source.lower(),
    )
    check(
        "Sin caracteres de guion problemáticos",
        not any(c in source for c in ('\u2011', '\u00ad', '\u2013', '\u2014')),
    )

    local_paths = [
        'README.md',
        'docs/language.md',
        'docs/ir-verification.md',
        'docs/circuit-abi.md',
        'docs/c-coverage.md',
        'docs/v1-plan.md',
        'src/cket.c',
        'src/cket_plugin.cpp',
        'src/cket_runtime.c',
        'src/cket_backend.c',
        'src/engine/verify.c',
        'src/engine/simulator.c',
        'src/engine/quantum.c',
        'tests/benchmark_synthesis.c',
        'tests/validate_qasm.c',
        'tests/fuzz_frontend.c',
        'include/quantum.h',
        'LICENSE',
    ]
    local_paths = sorted(
        set(
            local_paths
            + [
                'docs/c11-roadmap.md',
                'docs/c11-inventory.md',
                'docs/c11-library.json',
                'docs/c11-shifts.md',
                'docs/c11-expressions.md',
                'docs/c11-effect-values.md',
                'docs/c11-observations.md',
                'docs/c11-lazy-values.md',
                'docs/c11-inplace.md',
                'docs/c11-memcopy.md',
                'docs/c11-memcmp.md',
                'docs/c11-strncmp.md',
                'docs/c11-byte-snapshots.md',
                'docs/c11-memset.md',
                'docs/c11-memory-sequences.md',
                'docs/c11-controlled-operands.md',
                'docs/re-audit-2026-10-18-controlled-operands.md',
                'docs/re-audit-2026-10-17-memory-expressions.md',
                'docs/audit-evidence/memory-expressions/integration-summary.json',
                'docs/re-audit-2026-10-17-byte-snapshots.md',
                'docs/audit-evidence/byte-snapshots/integration-summary.json',
                'docs/estado-general-del-proyecto.es.md',
                'docs/re-audit-2026-10-15-strings.md',
                'docs/audit-evidence/strncmp/integration-summary.json',
                'docs/re-audit-2026-10-15-memory.md',
                'docs/audit-evidence/memcmp/comparison-summary.json',
                'docs/c11-division.md',
                'docs/c11-ctype.md',
                'tests/glibc-locale.lsan',
                'docs/c11-bindings.md',
                'docs/c11-history.md',
                'docs/c11-control.md',
                'docs/c11-cpu-control.md',
                'docs/audit-validation.md',
                'CONTRIBUTING.md',
                '.clang-format',
                '.editorconfig',
                'scripts/requirements-format.txt',
                'docs/repository-audit.md',
                'docs/re-audit-2026-10-14.md',
                'docs/audit-evidence/README.md',
                'docs/audit-evidence/re-audit-baseline.json',
                'docs/audit-evidence/numeric-locale.json',
                'docs/audit-evidence/profile-locale-division.json',
                'cket',
                *[
                    str(p.relative_to(ROOT))
                    for directory in ('src', 'include', 'tests', 'scripts', 'examples')
                    for p in (ROOT / directory).rglob('*')
                    if p.is_file()
                    and p.suffix in ('.c', '.cpp', '.h', '.inc', '.sh', '.py', '.json')
                ],
                *[str(p.relative_to(ROOT)) for p in (ROOT / '.github/workflows').glob('*.yml')],
            ]
        )
    )
    for path in local_paths:
        check("Fuente local " + path, (ROOT / path).is_file())
    check(
        "Límites físicos y simulación en código",
        all(
            x in (ROOT / 'src/engine/internal.h').read_text()
            for x in ('QC_MAX_PHYSICAL 128', 'QC_MAX_SIMULATED 24', 'QC_MAX_INPUT 12')
        ),
    )
    check(
        "Evento GCC en código",
        'PLUGIN_PRE_GENERICIZE' in (ROOT / 'src/cket_plugin.cpp').read_text(),
    )
    check(
        "Propiedad por atributo en header",
        '#define quant __attribute__((cket_quant))' in (ROOT / 'include/quantum.h').read_text(),
    )

    bell = json.loads((EVIDENCE / 'entanglement.state.json').read_text())
    probs = {int(a['index']): a['real'] ** 2 + a['imag'] ** 2 for a in bell['amplitudes']}
    check(
        "Bell: distribución conjunta ideal",
        set(probs) == {0, 3} and all(abs(v - 0.5) < 1e-10 for v in probs.values()),
    )
    search = json.loads((EVIDENCE / 'find.state.json').read_text())
    probs = {int(a['index']): a['real'] ** 2 + a['imag'] ** 2 for a in search['amplitudes']}
    formula = math.sin(7 * math.asin(math.sqrt(1 / 16))) ** 2
    check(
        "Grover: valor y fórmula",
        abs(probs.get(3, 0) - formula) < 1e-10 and abs(formula - 0.9613189697265625) < 1e-10,
    )
    check("Grover: auxiliares en cero", all(index < 256 for index in probs))
    check("Grover: normalización", abs(sum(probs.values()) - 1) < 1e-10)
    check("Memoria teórica: 24 y 32 qubits", 16 * 2**24 == 256 * 2**20 and 16 * 2**32 == 64 * 2**30)

    c_examples = re.findall(r'```c\n(.*?)```', source, re.S)
    check("Tres ejemplos C identificados", len(c_examples) == 3)
    c_example = c_examples[0]
    cpath = tmp / 'paper_hello.c'
    cpath.write_text(c_example)
    result = run([ROOT / 'build/cket', cpath, '--run', '--seed', '7', '-o', tmp / 'paper-hello'])
    check("Ejemplo exacto del manuscrito imprime 12", result.strip() == '12')
    for name, fragment, output_check in (
        (
            'bell',
            c_examples[1] + 'printf("%u%u\\n", first, second);\n',
            lambda s: s.strip() in ('00', '11'),
        ),
        ('find', c_examples[2], lambda s: s.strip() == '3'),
    ):
        path = tmp / f'paper_{name}.c'
        path.write_text(
            '#include <quantum.h>\n#include <stdio.h>\nint main(void) {\n'
            + fragment
            + 'return 0;\n}\n'
        )
        result = run(
            [ROOT / 'build/cket', path, '--run', '--seed', '7', '-o', tmp / f'paper-{name}']
        )
        check(
            "Fragmento exacto " + name + " compila y cumple el resultado con semilla 7",
            output_check(result),
        )
    ir_instruction = json.loads(re.search(r'```json\n(.*?)```', source, re.S)[1])
    doc = {
        'format': 'cket.qir',
        'version': 1,
        'instructions': [
            {'op': 'qalloc', 'id': 0, 'width': 8, 'signed': False},
            {'op': 'qprepare', 'id': 0, 'lower': 7, 'upper': 7},
            {'op': 'qalloc', 'id': 1, 'width': 8, 'signed': False},
            ir_instruction,
            {'op': 'qmeasure', 'id': 1},
            {'op': 'qmeasure', 'id': 0},
        ],
    }
    irpath = tmp / 'paper-sample.json'
    irpath.write_text(json.dumps(doc))
    run([ROOT / 'build/cket_backend', '--validate', irpath])
    state = json.loads(run([ROOT / 'build/cket_backend', '--state', irpath]))
    check(
        "IR del manuscrito calcula 7+5",
        len(state['amplitudes']) == 1 and state['amplitudes'][0]['index'] == 7 + (12 << 8),
    )

    pdf = fitz.open(PDFS[language])
    reader = PdfReader(PDFS[language])
    check(
        "PDF de ocho páginas como máximo, incluida bibliografía",
        1 <= len(pdf) <= 8,
        f"{len(pdf)} páginas",
    )
    check(
        "Tamaño Letter del documento de referencia",
        all(abs(p.rect.width - 612) < 0.01 and abs(p.rect.height - 792) < 0.01 for p in pdf),
    )
    text = '\n'.join(page.get_text() for page in pdf)
    normalized_text = re.sub(r'\s+', ' ', text)
    check("PDF sin notas internas de edición", not INTERNAL_EDITORIAL.search(normalized_text))
    compact_pages = [re.sub(r'\s+', '', page.get_text()) for page in pdf]
    code_blocks = re.findall(r'```(?:c|json)\n(.*?)```', source, re.S)
    check(
        "Ejemplos C y JSON íntegros en una página",
        all(
            any(re.sub(r'\s+', '', code) in page for page in compact_pages) for code in code_blocks
        ),
    )
    headings = re.findall(r'^#{2,3} (.+)$', source, re.M)
    page_bodies = [
        ' '.join(word[4] for word in page.get_text('words') if word[1] < 730) for page in pdf
    ]
    check(
        "Títulos unidos al contenido siguiente",
        all(not body.endswith(heading) for body in page_bodies for heading in headings),
    )
    for title in section_titles:
        check("Sección presente: " + title, title in normalized_text)
    for n in range(1, 8):
        check("Caption figura " + str(n), f'{figure_word} {n}.' in text)
    for n in range(1, 5):
        check("Caption tabla " + str(n), f'{table_word} {n}.' in text)
    check(
        "Diez referencias dentro del límite de páginas",
        references_heading[3:] in pdf[-1].get_text()
        and all(re.search(r'\[' + str(n) + r'\]', pdf[-1].get_text()) for n in range(1, 11)),
    )
    check(
        "Autor en PDF", 'Ignacio Schwindt' in text and reader.metadata.author == 'Ignacio Schwindt'
    )
    check(
        "Idioma del resumen y metadatos",
        ('Resumen' if language == 'es' else 'Abstract') in pdf[0].get_text()
        and reader.metadata.title == source.splitlines()[0][2:],
    )
    check("Sin glifos de reemplazo", '\ufffd' not in text and '\u25a0' not in text)
    links = [link for page in pdf for link in page.get_links()]
    check(
        "Referencias externas enlazadas",
        len([l for l in links if l.get('uri', '').startswith('https://')]) >= 10,
    )
    embedded_fonts = [font for page in pdf for font in page.get_fonts(full=True)]
    check(
        "Tipografía Times o STIX embebida",
        any(
            ('TimesNewRoman' in font[3] or 'STIXGeneral' in font[3]) and font[1] == 'ttf'
            for font in embedded_fonts
        ),
    )
    body_spans = [
        span
        for page in pdf
        for block in page.get_text('dict')['blocks']
        if block['type'] == 0
        for line in block['lines']
        for span in line['spans']
        if abs(span['size'] - 10) < 0.01 and len(span['text']) > 20
    ]
    check(
        "Texto principal serif de diez puntos y negro",
        len(body_spans) > 100
        and all(
            ('TimesNewRoman' in s['font'] or 'STIXGeneral' in s['font']) and s['color'] == 0
            for s in body_spans
        ),
    )
    check(
        "Título centrado de 17,2 puntos",
        any(
            abs(s['size'] - 17.2) < 0.01
            for b in pdf[0].get_text('dict')['blocks']
            if b['type'] == 0
            for line in b['lines']
            for s in line['spans']
        ),
    )
    for page in pdf:
        check("Página con contenido " + str(page.number + 1), len(page.get_text().strip()) > 400)
        words = page.get_text('words')
        body_words = [w for w in words if w[1] < 730]
        # Glyph bounds may extend fractionally beyond ReportLab's line metrics.
        outside = [w for w in body_words if w[0] < 107 or w[2] > 505 or w[1] < 68 or w[3] > 723]
        check(
            "Columna de 5,5 pulgadas y márgenes de página " + str(page.number + 1),
            not outside,
            str(outside[:2]),
        )
        footer = [w for w in words if w[1] >= 730]
        check(
            "Número centrado y único en página " + str(page.number + 1),
            len(footer) == 1
            and footer[0][4] == str(page.number + 1)
            and abs((footer[0][0] + footer[0][2]) / 2 - 306) < 1,
        )
    reference_layout = {
        'reference_url': 'https://arxiv.org/pdf/1706.03762',
        'reference_title': 'Attention Is All You Need',
        'reference_pdf_version': 'v7, 2 August 2023',
        'page_size_points': [612, 792],
        'text_column_points': [108, 504],
        'body_font_reference': 'NimbusRomNo9L (Times compatible), approximately 10 pt',
        'body_font_cket': 'Embedded Times New Roman or STIXGeneral, 10 pt',
        'heading_points': 12,
        'title_points': 17.2,
        'structure': [
            'Introduction',
            'Background',
            'Architecture',
            'Motivation',
            'Experimental setup',
            'Results',
            'Conclusion',
            'References',
        ],
        'adaptation': 'Spanish and English compiler content; original C Ket figures; no copied branding or affiliations',
        'maximum_pages_including_references': 8,
    }
    write_json(EVIDENCE / 'layout-reference.json', reference_layout, ensure_ascii=False)
    toolchain = {
        p: importlib.metadata.version(p)
        for p in ('reportlab', 'matplotlib', 'pillow', 'pymupdf', 'pypdf', 'pdfplumber')
    }
    write_json(EVIDENCE / 'editorial-toolchain.json', toolchain)
    current_campaign = json.loads(
        (ROOT / 'docs/audit-evidence/memory-expressions/results.json').read_text()
    )
    counts = current_campaign['counts_per_configuration']
    current_states = counts['external_qasm_states']
    current_circuits = current_states + counts['external_qasm_channels']
    current_readme = (ROOT / 'README.md').read_text()
    check(
        'README: current independent circuit and state counts match completed evidence',
        current_campaign['status'] in ('passed', 'passed-in-segments')
        and bool(re.search(rf'\b{current_circuits}\s+independent QASM cases\b', current_readme))
        and bool(
            re.search(rf'QASM cases check {current_states}\s+complete states\b', current_readme)
        ),
    )
    check(
        'Coverage: current independent circuit and state counts match completed evidence',
        bool(
            re.search(
                rf'\b{current_circuits}\s+exported QASM\s+circuits:\s+{current_states}\s+state cases\b',
                (ROOT / 'docs/c-coverage.md').read_text(),
            )
        ),
    )
    current_inventory = json.loads((ROOT / 'docs/c11-library.json').read_text())['interfaces']
    overview = (ROOT / 'docs/estado-general-del-proyecto.es.md').read_text()
    current_totals = {
        status: sum(row['quantum'] == status for row in current_inventory)
        for status in ('pending', 'partial', 'metadata-only', 'implemented')
    }
    check(
        'Spanish overview: inventory table matches actual interface classifications',
        len(current_inventory) == 1224
        and current_totals['implemented'] == 0
        and all(
            re.search(rf'\|\s*{label}\s*\|\s*{current_totals[status]}\s*\|', overview)
            for label, status in [
                ('Pendientes', 'pending'),
                ('Parciales', 'partial'),
                ('Solo metadatos', 'metadata-only'),
            ]
        ),
    )
    partial_functions = sum(
        row['quantum'] == 'partial' and row['kind'] == 'function' for row in current_inventory
    )
    partial_types = sum(
        row['quantum'] == 'partial' and row['kind'] == 'type' for row in current_inventory
    )
    check(
        'Spanish overview: partial function/type counts remain distinct from complete C11',
        bool(
            re.search(
                rf'\b{partial_functions} nombres de funciones con adaptación parcial\b', overview
            )
        )
        and bool(re.search(rf'\b{partial_types} entradas de tipos y alias\b', overview))
        and 'Todavía no implementa compatibilidad cuántica completa con C11'
        in re.sub(r'\s+', ' ', overview),
    )
    previous_integration = json.loads(
        (
            ROOT / 'docs/audit-evidence/coherent-memory-integration/integration-summary.json'
        ).read_text()
    )
    current_integration = json.loads(
        (
            ROOT / 'docs/audit-evidence/controlled-operands-integration/integration-summary.json'
        ).read_text()
    )
    current_full = json.loads(
        (ROOT / 'docs/audit-evidence/controlled-operands-full/summary.json').read_text()
    )
    previous_full = json.loads(
        (ROOT / 'docs/audit-evidence/coherent-control-full/summary.json').read_text()
    )
    check(
        'Historical138 integration matches its targeted installation and completed broad sources',
        current_integration['root_verification']['status'] == 'passed-targeted'
        and current_integration['source_files'] == len(current_integration['source_sha256']) == 138
        and current_integration['source_sha256'] == current_full['source_sha256']
        and current_full['status'] == 'verified-broad-campaign'
        and current_full['exit_code'] == 0
        and current_full['closed_commands'] == 76
        and all(
            row['native_checks'] == 393
            and row['controlled_operand_checks'] == 1270
            and row['exact_contract_executions'] == 40
            for row in current_integration['root_verification']['configurations']
        ),
    )
    check(
        'Historical137 broad campaign completed separately from its original targeted integration metadata',
        previous_full['status'] == 'verified-broad-campaign'
        and previous_full['exit_code'] == 0
        and previous_full['closed_commands'] == 74
        and previous_full['source_sha256'] == previous_integration['source_sha256']
        and previous_integration['source_files'] == 137
        and previous_integration['source_sha256'] != current_integration['source_sha256'],
    )
    coherent_audit = json.loads(
        (ROOT / 'docs/audit-evidence/coherent-cpu-conditions/experimental-summary.json').read_text()
    )
    reproduced = coherent_audit['main_bug']
    check(
        'Current audit: historical inactive-condition bug and later positive integration remain distinct',
        reproduced['status'] == 'accepted-program-bug-reproduced'
        and reproduced['source_files'] == 134
        and reproduced['source_sha256'] != current_integration['source_sha256']
        and reproduced['ordinary_c_returncode'] == 0
        and len(reproduced['rows']) == 10
        and all(
            row['compile_returncode'] == 0 and row['run_returncode'] == -8
            for row in reproduced['rows']
        )
        and all(
            row['cpu_condition_native_executions'] == 180
            for row in previous_integration['root_verification']['configurations']
        )
        and 'corrección integrada' in re.sub(r'\s+', ' ', overview),
    )
    coherent_maintained = json.loads(
        (
            ROOT / 'docs/audit-evidence/coherent-cpu-conditions-maintained/maintained-summary.json'
        ).read_text()
    )
    check(
        'Integrated audit: scoped coherent-control evidence does not imply complete C11 compatibility',
        coherent_maintained['status'] == 'verified-scoped-maintained'
        and coherent_maintained['source_files'] == 137
        and coherent_maintained['source_sha256'] == previous_integration['source_sha256']
        and coherent_audit['variants']['v3']['independent_complex_states'] == 800
        and coherent_audit['variants']['v3']['native_executions_per_configuration'] == 180,
    )
    arm_failures = json.loads(
        (ROOT / 'docs/audit-evidence/coherent-arm-operands/results.json').read_text()
    )['rows']
    check(
        'Historical137 audit: seven original controlled-body failures retain their source identity',
        len(arm_failures) == 8
        and sum(row['corrected137'].get('run_returncode') == -8 for row in arm_failures) == 7
        and all(row['ordinary_c_returncode'] == 0 for row in arm_failures)
        and 'siete fallos' in re.sub(r'\s+', ' ', overview),
    )
    controlled = json.loads(
        (ROOT / 'docs/audit-evidence/controlled-operands/experimental-summary.json').read_text()
    )
    controlled_v4 = controlled['versions']['v4']
    controlled_maintained = json.loads(
        (
            ROOT / 'docs/audit-evidence/controlled-operands-maintained/maintained-summary.json'
        ).read_text()
    )
    controlled_failure = json.loads(
        (
            ROOT / 'docs/audit-evidence/controlled-operands-regression-failure/failure-summary.json'
        ).read_text()
    )
    check(
        'Candidate operands: finite native and complete-state scopes remain distinct from main',
        controlled_v4['status'] == 'verified-scoped'
        and all(
            row['defined_native_executions'] == 1000 and row['undefined_domain_diagnostics'] == 200
            for row in controlled_v4['native_configurations']
        )
        and sum(row['checks'] for row in controlled_v4['states'].values()) == 8000
        and controlled_v4['source_sha256'] != current_integration['source_sha256'],
    )
    check(
        'Candidate operands: maintained C suite does not certify full compatibility',
        controlled_maintained['status'] == 'verified-scoped-maintained'
        and controlled_maintained['source_files'] == 138
        and sum(
            value
            for key, value in controlled_maintained['checks_per_configuration'].items()
            if key != 'ordinary_c_executions_separate'
        )
        == 1270
        and all(
            controlled_maintained['source_sha256'][name] == controlled_v4['source_sha256'][name]
            for name in ('src/quantum_frontend.inc', 'src/quantum_semantics.inc')
        ),
    )
    check(
        'Historical v4 operands: two actual broad failures prevented promotion of those sources',
        controlled_failure['status'] == 'failed-broad-candidate-not-integrated'
        and controlled_failure['exit_code'] == 1
        and controlled_failure['source_sha256'] == controlled_maintained['source_sha256']
        and controlled_failure['native_passed'] == 391
        and controlled_failure['native_failed'] == 2
        and [row['returncode'] for row in controlled_failure['commands']] == [0, 1]
        and controlled_failure['source_sha256'] != current_integration['source_sha256'],
    )
    operand_v5 = json.loads(
        (ROOT / 'docs/audit-evidence/controlled-operands-v5/summary.json').read_text()
    )
    integrated_ci = json.loads(
        (ROOT / 'docs/audit-evidence/integrated137-remote-ci/summary.json').read_text()
    )
    current_ci = json.loads(
        (ROOT / 'docs/audit-evidence/integrated138-remote-ci/summary.json').read_text()
    )
    deferred_phase = json.loads(
        (ROOT / 'docs/audit-evidence/deferred-phase-v6/summary.json').read_text()
    )
    guarded_loads = json.loads(
        (ROOT / 'docs/audit-evidence/guarded-cpu-loads-v3/summary.json').read_text()
    )
    phase_broad_failure = json.loads(
        (ROOT / 'docs/audit-evidence/deferred-phase-v6-broad-failure/summary.json').read_text()
    )
    check(
        'V5 candidate: positive native and independent inverse scopes preserve the earlier runner failure',
        operand_v5['status'] == 'verified-scoped-v5-not-integrated'
        and operand_v5['source_files'] == 138
        and operand_v5['native_checks_per_configuration'] == 393
        and operand_v5['controlled_operand_checks_per_configuration'] == 1270
        and sum(row['complete_state_checks'] for row in operand_v5['operand_states'].values())
        == 8000
        and sum(
            row['complete_state_checks'] for row in operand_v5['reversible_regressions'].values()
        )
        == 1200
        and sum(row['native_executions'] for row in operand_v5['reversible_regressions'].values())
        == 600
        and operand_v5['reversible_regressions']['normal']['original_runner_exit_code'] == 1
        and operand_v5['reversible_regressions']['sanitized']['original_runner_exit_code'] == 0,
    )
    check(
        'Integrated v5: pure CPU loads still fault despite valid ordinary C inactive arms',
        operand_v5['pending_cpu_loads']['ordinary_c_executions'] == 70
        and operand_v5['pending_cpu_loads']['accepted_signals'] == 140
        and operand_v5['pending_cpu_loads']['profiles'] == 7
        and operand_v5['source_sha256'] == current_integration['source_sha256'],
    )
    check(
        'Integrated137 CI: completed repository workflow remains distinct from v5 and larger local fuzzing',
        integrated_ci['status'] == 'verified-remote-ci-passed'
        and integrated_ci['source_sha256'] == previous_integration['source_sha256']
        and integrated_ci['normal_and_sanitized']['fuzz_compilations_each'] == 145
        and integrated_ci['normal_and_sanitized']['fuzz_seed'] == 20261011,
    )
    check(
        'Historical138 CI: the completed workflow verifies that source integration',
        current_ci['status'] == 'verified-remote-ci-passed'
        and current_ci['source_files'] == 138
        and current_ci['source_sha256'] == current_integration['source_sha256']
        and current_ci['normal_and_sanitized']['native_each'] == 393
        and current_ci['normal_and_sanitized']['controlled_operand_checks_each'] == 1270
        and current_ci['normal_and_sanitized']['fuzz_compilations_each'] == 145
        and current_ci['normal_and_sanitized']['fuzz_seed'] == 20261011
        and all(job['conclusion'] == 'success' for job in current_ci['jobs']),
    )
    check(
        'Historical phase143 candidate: closed proofs remain distinct from its failed broad regression',
        deferred_phase['status'] == 'verified-scoped-phase-not-integrated'
        and deferred_phase['source_files'] == 143
        and deferred_phase['integrated_base_source_sha256'] == current_integration['source_sha256']
        and deferred_phase['checks_per_configuration']['total'] == 3380
        and sum(
            row['complete_state_checks'] for row in deferred_phase['independent_states'].values()
        )
        == 1600
        and deferred_phase['history']['v2_sanitized_failed'] == 170
        and deferred_phase['history']['v2_original_state_runner_exit'] == 1
        and deferred_phase['broad_campaign_at_archive']['status'] == 'running',
    )
    check(
        'Historical guarded CPU loads: closed targeted143 matrices retain their original scope',
        guarded_loads['status'] == 'verified-targeted-cpu-loads-not-integrated'
        and guarded_loads['source_inputs'] == 143
        and guarded_loads['closed_targeted_commands'] == 12
        and guarded_loads['source_sha256'] != current_integration['source_sha256']
        and guarded_loads['per_configuration']
        == {'native': 393, 'controlled_operands': 1270, 'phase': 3380, 'query': 76}
        and all(
            row == {'rows': 140, 'native_executions': 420, 'ordinary_c_executions': 420}
            for row in guarded_loads['native_matrices'].values()
        )
        and set(guarded_loads['native_matrices']) == {'normal', 'sanitized'}
        and not guarded_loads['security_changes'],
    )
    check(
        'Guarded CPU loads: all scoped artifacts match their recorded hashes and sizes',
        all(
            hashlib.sha256(
                (ROOT / 'docs/audit-evidence/guarded-cpu-loads-v3' / name).read_bytes()
            ).hexdigest()
            == expected['sha256']
            and (ROOT / 'docs/audit-evidence/guarded-cpu-loads-v3' / name).stat().st_size
            == expected['bytes']
            for name, expected in guarded_loads['artifacts'].items()
        ),
    )
    string_integration = json.loads(
        (ROOT / 'docs/audit-evidence/strings-integration-v6/integration-summary.json').read_text()
    )
    with tarfile.open(
        ROOT / 'docs/audit-evidence/strings-integration-v6/sources.tar.gz'
    ) as archive:
        archived144 = {
            name: hashlib.sha256(archive.extractfile(name).read()).hexdigest()
            for name in string_integration['source_sha256']
        }
    check(
        'Historical144 compiler: exact archived inputs have closed targeted checks; larger local regression remains separate',
        string_integration['status'] == 'verified-targeted-applied-broad-pending'
        and string_integration['source_files'] == len(string_integration['source_sha256']) == 144
        and string_integration['targeted_commands_closed_zero'] == 18
        and string_integration['exact_documented_program_executions'] == 40
        and string_integration['broad_campaign_at_archive']['status'] == 'running'
        and string_integration['broad_campaign_at_archive']['planned_commands'] == 80
        and archived144 == string_integration['source_sha256']
        and [
            name
            for name, digest in string_integration['source_sha256'].items()
            if digest != string_integration['tested_source_sha256'][name]
        ]
        == ['scripts/archive-audit.py'],
    )
    check(
        'Historical144 strings and CPU loads: independent complex states and native load matrices keep separate scopes',
        string_integration['checks_per_configuration']
        == {
            'bindings': 519,
            'strings': 460,
            'native': 393,
            'controlled_operands': 1270,
            'memcmp': 302,
            'strncmp': 246,
            'library': 2100,
            'phase': 3380,
        }
        and set(string_integration['independent_compiled_string_states']) == {'normal', 'sanitized'}
        and all(
            row['captures'] == 160
            and row['ordinary_c_executions'] == 80
            and row['complete_state_checks'] == 320
            and row['maximum_amplitude_error'] < 1e-10
            for row in string_integration['independent_compiled_string_states'].values()
        )
        and set(string_integration['current_cpu_load_matrix']) == {'normal', 'sanitized'}
        and all(
            row == {'rows': 140, 'native_executions': 420, 'ordinary_c_executions': 420}
            for row in string_integration['current_cpu_load_matrix'].values()
        ),
    )
    check(
        'Historical144 CPU loads: distinct source phases validate complete states and derived-result inverses separately from update inverses',
        set(string_integration['independent_compiled_cpu_load_states']) == {'normal', 'sanitized'}
        and all(
            row['captures'] == 280
            and row['ordinary_c_executions'] == 140
            and row['complete_state_checks'] == 560
            and row['maximum_amplitude_error'] < 1e-10
            for row in string_integration['independent_compiled_cpu_load_states'].values()
        ),
    )
    current_index = json.loads((ROOT / 'docs/c11-library.json').read_text())
    check(
        'Historical144 inventory remains unchanged; current string adapters remain partial without claiming complete C11',
        string_integration['inventory']
        == {
            'headers': 29,
            'entries': 1224,
            'pending': 780,
            'partial': 76,
            'metadata_only': 368,
            'partial_function_names': 29,
            'complete': 0,
        }
        and len(current_index['interfaces']) == 1224
        and sum(row['quantum'] == 'pending' for row in current_index['interfaces']) == 773
        and sum(row['quantum'] == 'partial' for row in current_index['interfaces']) == 83
        and all(
            row['quantum'] == 'partial'
            for row in current_index['interfaces']
            if row['header'] == 'string.h'
            and row['name']
            in {
                'strlen',
                'strcmp',
                'strspn',
                'strcspn',
                'memchr',
                'strchr',
                'strrchr',
                'strpbrk',
                'strstr',
            }
        ),
    )
    counter_correction = json.loads(
        (ROOT / 'docs/audit-evidence/memset-counter-correction/summary.json').read_text()
    )
    check(
        'Memset category correction: exact normal/sanitized logs record 350 native cases and 70 diagnostics, total 800',
        counter_correction['status'] == 'verified-documentation-counter-correction'
        and counter_correction['source_sha256'] == string_integration['source_sha256']
        and counter_correction['checks_per_configuration']['native_executions'] == 350
        and counter_correction['checks_per_configuration']['diagnostic_contracts'] == 70
        and sum(counter_correction['checks_per_configuration'].values())
        == counter_correction['total_per_configuration']
        == 800
        and len(counter_correction['commands']) == 2
        and all(
            row['returncode'] == 0
            and hashlib.sha256(
                (ROOT / 'docs/audit-evidence/memset-counter-correction' / row['log']).read_bytes()
            ).hexdigest()
            == row['log_sha256']
            for row in counter_correction['commands']
        )
        and hashlib.sha256((ROOT / counter_correction['source_archive']).read_bytes()).hexdigest()
        == counter_correction['source_archive_sha256'],
    )
    integrated144_ci = json.loads(
        (ROOT / 'docs/audit-evidence/integrated144-remote-ci/summary.json').read_text()
    )
    check(
        'Integrated144 CI: exact committed inputs passed 28 normal/sanitizer suites; expanded local fuzzing remains separate',
        integrated144_ci['status'] == 'verified-remote-ci-passed'
        and integrated144_ci['head_sha'] == '45f71e7c6f16b0555d4ca693e2a9ae332d995844'
        and integrated144_ci['source_files'] == 144
        and integrated144_ci['source_sha256'] == string_integration['source_sha256']
        and integrated144_ci['verified_suite_patterns'] == 28
        and integrated144_ci['normal_and_sanitized']['strings_each'] == 460
        and integrated144_ci['normal_and_sanitized']['bindings_each'] == 519
        and integrated144_ci['normal_and_sanitized']['phase_each'] == 3380
        and integrated144_ci['normal_and_sanitized']['fuzz_compilations_each'] == 145
        and integrated144_ci['normal_and_sanitized']['fuzz_seed'] == 20261011
        and all(job['conclusion'] == 'success' for job in integrated144_ci['jobs']),
    )
    fill_integration = json.loads(
        (ROOT / 'docs/audit-evidence/known-fill-cleanup-v2/integration-summary.json').read_text()
    )
    fill_segments = json.loads(
        (ROOT / 'docs/audit-evidence/known-fill-cleanup-v2/targeted-segments.json').read_text()
    )
    with tarfile.open(ROOT / 'docs/audit-evidence/known-fill-cleanup-v2/sources.tar.gz') as archive:
        archived146 = {
            name: hashlib.sha256(archive.extractfile(name).read()).hexdigest()
            for name in fill_integration['source_sha256']
        }
    check(
        'Historical146 compiler: exact archived inputs passed targeted checks and fresh root contracts; full regression stays separate',
        fill_integration['status'] == 'verified-targeted-segmented-applied-broad-pending'
        and fill_integration['source_files'] == len(fill_integration['source_sha256']) == 146
        and fill_integration['previous_integrated_source_sha256'] == archived144
        and len(fill_integration['changed_sources']) == 6
        and fill_integration['checks_per_configuration']
        == {
            'memset': 940,
            'native': 393,
            'controlled_operands': 1270,
            'history': 2332,
            'inplace': 271,
            'memory_sequences': 360,
        }
        and fill_integration['root_verification']['fresh_root_build'] == 'passed'
        and fill_integration['root_verification']['exact_program_executions'] == 20
        and archived146 == fill_integration['source_sha256']
        and all(
            hashlib.sha256(
                (ROOT / 'docs/audit-evidence/known-fill-cleanup-v2' / (name + '.c')).read_bytes()
            ).hexdigest()
            == digest
            for name, digest in fill_integration['root_verification'][
                'exact_program_source_sha256'
            ].items()
        ),
    )
    check(
        'Historical146 scope proof: full states check actual compiler cleanup; explicit C inverses and unknown retained roots keep their scope',
        set(fill_integration['independent_compiled_scope_states']) == {'normal', 'sanitized'}
        and all(
            row['captures'] == 588
            and row['ordinary_c_executions'] == 294
            and row['complete_state_checks'] == 1764
            and row['maximum_amplitude_error'] < 1e-10
            for row in fill_integration['independent_compiled_scope_states'].values()
        ),
    )
    check(
        'Historical146 segmented qualification: previous processes have no fabricated success; only unclosed suites repeat',
        fill_integration['targeted_commands_closed_zero'] == 56
        and set(fill_segments) == {'normal', 'sanitized'}
        and all(
            row['initial']['status'] == 'running'
            and row['continuation']['status'] == 'passed-in-segments'
            and row['continuation']['exit_code'] == 0
            and row['initial']['source_sha256']
            == row['continuation']['source_sha256']
            == fill_integration['source_sha256']
            and row['continuation']['previous_job']['boot_id']
            != row['continuation']['current_boot_id']
            and len(row['initial']['commands']) + len(row['continuation']['commands']) == 28
            and all(
                command['returncode'] == 0
                for command in [*row['initial']['commands'], *row['continuation']['commands']]
            )
            for row in fill_segments.values()
        ),
    )
    check(
        'Historical146 signed fill: checked word prefix stays separate from failed64-bit resource experiment',
        set(fill_integration['signed_word_prefix_and_resource_failure']) == {'normal', 'sanitized'}
        and all(
            row['exit_code'] == 1
            and row['completed_captures_before_failure'] == 100
            and row['completed_state_checks_before_failure'] == 300
            and row['validated_prefix_widths'] == [8, 16, 32]
            and 'limited to 128 physical qubits' in row['error']
            for row in fill_integration['signed_word_prefix_and_resource_failure'].values()
        ),
    )
    remainder = json.loads(
        (ROOT / 'docs/audit-evidence/common-remainder-v2/integration-summary.json').read_text()
    )
    with tarfile.open(ROOT / 'docs/audit-evidence/common-remainder-v2/sources.tar.gz') as archive:
        archived148 = {
            name: hashlib.sha256(archive.extractfile(name).read()).hexdigest()
            for name in remainder['source_sha256']
        }
    with tarfile.open(
        ROOT / 'docs/audit-evidence/common-remainder-v2/proof147-sources.tar.gz'
    ) as archive:
        archived147 = {
            name: hashlib.sha256(archive.extractfile(name).read()).hexdigest()
            for name in remainder['proof147_source_sha256']
        }
    check(
        'Historical148 remainder: exact archived bytes and fresh normal/sanitizer targeted regressions; broad verification separate',
        remainder['status'] == 'verified-targeted-applied-broad-pending'
        and remainder['source_files'] == len(archived148) == 148
        and archived148 == remainder['source_sha256']
        and archived146 == remainder['previous_integrated_source_sha256']
        and len(remainder['changed_sources_from146']) == 5
        and remainder['checks_per_configuration']
        == {'expressions': 907, 'division': 990, 'native': 393, 'inplace': 271}
        and remainder['new_finite_basis_action_checks_per_configuration'] == 810
        and remainder['targeted_commands_closed_zero'] == 10
        and remainder['broad_regression'] == remainder['remote_ci'] == 'pending',
    )
    check(
        'Historical148 remainder: byte-state proof keeps its147 identity; every production input is byte-identical',
        len(archived147) == 147
        and archived147 == remainder['proof147_source_sha256']
        and len(remainder['changed_sources_from_proof147']) == 3
        and set(remainder['production_sources_identical_to_proof147'])
        == {name for name in archived148 if name.startswith(('src/', 'include/')) or name == 'cket'}
        and all(
            archived147[name] == archived148[name]
            for name in remainder['production_sources_identical_to_proof147']
        )
        and set(remainder['independent_compiled_byte_states_on_production_identical147'])
        == {'normal', 'sanitized'}
        and all(
            row['captures'] == 240
            and row['ordinary_c_executions'] == 120
            and row['complete_state_checks'] == 720
            and row['maximum_amplitude_error'] < 1e-10
            for row in remainder[
                'independent_compiled_byte_states_on_production_identical147'
            ].values()
        ),
    )
    check(
        'Historical148 remainder: wide edges, mutable CPU-divisor snapshots, phases and actual scope inverses',
        set(remainder['independent_compiled_edge_states_on148']) == {'normal', 'sanitized'}
        and all(
            row['captures'] == 88
            and row['ordinary_c_executions'] == 44
            and row['complete_state_checks'] == 264
            and row['maximum_amplitude_error'] < 1e-10
            for row in remainder['independent_compiled_edge_states_on148'].values()
        )
        and remainder['common_type_profiles']
        == {
            'baseline_captures': 632,
            'baseline_resource_limits': 4,
            'current_captures': 640,
            'ordinary_c_executions': 320,
            'status': 'finite-profiles-passed-not-type-completeness',
        },
    )
    check(
        'Historical148 remainder: full-width conditional64 preparation failure is preserved separately from later wire-view proofs',
        set(remainder['conditional64_resource_failure']) == {'normal', 'sanitized'}
        and all(
            row['exit_code'] == 1
            and row['completed_captures_before_failure'] == 24
            and row['completed_state_checks_before_failure'] == 72
            and 'limited to 128 physical qubits' in row['error']
            for row in remainder['conditional64_resource_failure'].values()
        ),
    )
    historical144 = remainder['historical144_completed_broad']
    with tarfile.open(
        ROOT / 'docs/audit-evidence/common-remainder-v2/historical144-broad.tar.gz'
    ) as archive:
        historical144_progress = json.load(archive.extractfile('continuation/progress.json'))
        historical144_sources = json.load(archive.extractfile('sources.json'))
        historical144_fuzz = {
            configuration: json.load(
                archive.extractfile('continuation/' + configuration + '-fuzz-coverage.json')
            )
            for configuration in ('normal', 'sanitized')
        }
        historical144_logs_match = all(
            row['returncode'] == 0
            and hashlib.sha256(archive.extractfile('continuation/' + row['log']).read()).hexdigest()
            == row['log_sha256']
            for row in historical144_progress['commands']
        )
        historical144_original_unclosed = 'original/exit.json' not in archive.getnames()
    check(
        'Historical144 broad completion:80 real zero commands,59 reused plus21 continued;665 fuzz compilations each; current148 remains separate',
        historical144['status'] == historical144_progress['status'] == 'passed-in-segments'
        and historical144['exit_code'] == historical144_progress['exit_code'] == 0
        and historical144['source_sha256'] == historical144_sources == archived144
        and historical144['closed_commands'] == len(historical144_progress['commands']) == 80
        and historical144['reused_original_closed_commands'] == 59
        and historical144['new_continuation_closed_commands'] == 21
        and sum(
            row.get('reused_closed_command_from_previous_segment', False)
            for row in historical144_progress['commands']
        )
        == 59
        and historical144_original_unclosed
        and historical144_logs_match
        and historical144_progress['continuation']['previous_boot_id']
        != historical144_progress['continuation']['current_boot_id']
        and all(
            row['status'] == 'passed-in-segments'
            and row['compilations'] == 665
            and row['seed'] == 20261012
            and row['case_indices'] == list(range(133))
            and row['optimization_levels'] == 5
            and row['byte_inputs_per_compilation'] == 256
            for row in historical144_fuzz.values()
        ),
    )
    selection = json.loads(
        (ROOT / 'docs/audit-evidence/selection-views-v5/integration-summary.json').read_text()
    )
    with tarfile.open(ROOT / 'docs/audit-evidence/selection-views-v5/sources.tar.gz') as archive:
        archived151 = {
            name: hashlib.sha256(archive.extractfile(name).read()).hexdigest()
            for name in selection['source_sha256']
        }
    with tarfile.open(
        ROOT / 'docs/audit-evidence/selection-views-v5/proof-sources.tar.gz'
    ) as archive:
        selection_proof_sources = {
            name: hashlib.sha256(archive.extractfile(name).read()).hexdigest()
            for name in selection['proof_source_sha256']
        }
    selection_qualification = json.loads(
        (ROOT / 'docs/audit-evidence/selection-views-v5/qualification.json').read_text()
    )
    selection_inherited_builds = json.loads(
        (ROOT / 'docs/audit-evidence/selection-views-v5/inherited-fresh-builds.json').read_text()
    )
    check(
        'Historical151 selection: exact archived source bytes,1195 expression checks and five targeted suites per configuration',
        selection['status'] == 'verified-targeted-production-identical-proofs-broad-pending'
        and selection['source_files'] == len(archived151) == 151
        and archived151 == selection['source_sha256']
        and archived148 == selection['previous_integrated_source_sha256']
        and len(selection['changed_sources_from148']) == 7
        and selection['checks_per_configuration']
        == {'expressions': 1195, 'shifts': 55, 'division': 990, 'native': 393, 'inplace': 271}
        and selection['new_finite_selection_domains_per_configuration'] == 288
        and selection['targeted_commands_closed_zero'] == 12,
    )
    check(
        'Historical151 selection: verified reused cores keep their fresh-build provenance and earlier failed test status',
        set(selection_qualification) == set(selection_inherited_builds) == {'normal', 'sanitized'}
        and all(
            row['status'] == 'passed-targeted-selection-regression'
            and row['exit_code'] == 0
            and row['source_sha256'] == archived151
            and len(row['commands']) == 6
            and row['commands'][0]['name'] == 'rebuild-expression-validator'
            and all(command['returncode'] == 0 for command in row['commands'])
            and row['tool_reuse']['production_inputs_byte_identical'] is True
            and row['core_sha256'] == selection_inherited_builds[configuration]['core_sha256']
            and selection_inherited_builds[configuration]['status']
            == 'failed-targeted-selection-regression'
            and selection_inherited_builds[configuration]['exit_code'] == 1
            and selection_inherited_builds[configuration]['commands'][0]['name'] == 'fresh-build'
            and selection_inherited_builds[configuration]['commands'][0]['returncode'] == 0
            for configuration, row in selection_qualification.items()
        ),
    )
    check(
        'Historical151 selection:768 full compiled states keep a production-identical proof snapshot; only the test budget changed',
        len(selection_proof_sources) == 151
        and selection_proof_sources == selection['proof_source_sha256']
        and selection['changed_sources_from_proof'] == ['tests/selection_state.inc']
        and [name for name in archived151 if archived151[name] != selection_proof_sources[name]]
        == ['tests/selection_state.inc']
        and set(selection['production_sources_identical_to_proof'])
        == {name for name in archived151 if name.startswith(('src/', 'include/')) or name == 'cket'}
        and all(
            archived151[name] == selection_proof_sources[name]
            for name in selection['production_sources_identical_to_proof']
        )
        and set(selection['independent_compiled_states']) == {'normal', 'sanitized'}
        and all(
            set(rows) == {'extremes', 'shared_columns'}
            and all(
                row['captures'] == 64
                and row['ordinary_c_executions'] == 32
                and row['complete_state_checks'] == 192
                and row['maximum_amplitude_error'] < 1e-10
                for row in rows.values()
            )
            for rows in selection['independent_compiled_states'].values()
        )
        and max(
            row['maximum_physical_wires']
            for rows in selection['independent_compiled_states'].values()
            for row in rows.values()
        )
        == 102,
    )
    check(
        'Historical151 selection: incorrect oracle/budget attempts remain failed; broad-campaign snapshot is not a pass',
        set(selection['earlier_test_attempts']) == {'v2', 'v3', 'v4'}
        and all(
            set(rows) == {'normal', 'sanitized'}
            and all(
                row['status'] == 'failed-targeted-selection-regression' and row['exit_code'] == 1
                for row in rows.values()
            )
            for rows in selection['earlier_test_attempts'].values()
        )
        and selection['broad_campaign_status_at_archive'] == 'running'
        and selection['broad_regression'] == selection['remote_ci'] == 'pending',
    )
    division_folder = ROOT / 'docs/audit-evidence/common-division-v3'
    division_delivery = json.loads((division_folder / 'integration-summary.json').read_text())
    division_qualification = json.loads((division_folder / 'qualification.json').read_text())
    delivery_record = json.loads((division_folder / 'delivery.json').read_text())
    with tarfile.open(division_folder / 'evidence.tar.gz') as archive:
        division_sources = {
            name: hashlib.sha256(archive.extractfile('delivery-sources/' + name).read()).hexdigest()
            for name in division_delivery['source_sha256']
        }
        division_qualified_sources = {
            name: hashlib.sha256(
                archive.extractfile('qualification-sources/' + name).read()
            ).hexdigest()
            for name in division_delivery['qualified_source_sha256']
        }
        division_logs_match = all(
            hashlib.sha256(
                archive.extractfile('records/' + label + '/' + command['log']).read()
            ).hexdigest()
            == command['log_sha256']
            for label, record in {**division_qualification, 'delivery': delivery_record}.items()
            for command in record['commands']
        )
    check(
        'Historical delivery155: exact archived source identity, fresh Linux build, eight runnable exports and1509 example checks',
        division_delivery['status'] == 'passed-usable-delivery-and-targeted-division'
        and division_delivery['source_files'] == len(division_sources) == 155
        and division_sources
        == division_delivery['source_sha256']
        == delivery_record['source_sha256']
        and delivery_record['status'] == 'passed-delivery-one'
        and delivery_record['exit_code'] == 0
        and [row['name'] for row in delivery_record['commands']]
        == ['fresh-build', 'block01', 'examples']
        and all(row['returncode'] == 0 for row in delivery_record['commands'])
        and division_delivery['delivery']['native_programs'] == 8
        and division_delivery['delivery']['qasm_exports'] == 8
        and division_delivery['delivery']['example_checks'] == 1509
        and division_delivery['delivery']['examples'] == 37
        and division_logs_match,
    )
    check(
        'Division153:2310 new finite domains and3505 expression checks per configuration; reused cores retain152-input provenance',
        len(division_qualified_sources) == 153
        and division_qualified_sources == division_delivery['qualified_source_sha256']
        and len(division_delivery['fresh_core_source_sha256']) == 152
        and division_delivery['new_finite_quotient_domains_per_configuration'] == 2310
        and division_delivery['checks_per_configuration']
        == {'expressions': 3505, 'shifts': 55, 'division': 990, 'native': 393, 'inplace': 271}
        and all(
            division_qualified_sources[name]
            == division_sources[name]
            == division_delivery['fresh_core_source_sha256'][name]
            for name in division_delivery['production_sources_identical']
        )
        and all(
            division_qualification['qualification-' + configuration]['status']
            == 'passed-targeted-common-division'
            and division_qualification['qualification-' + configuration]['source_sha256']
            == division_qualified_sources
            and len(division_qualification['qualification-' + configuration]['commands']) == 6
            and all(
                row['returncode'] == 0
                for row in division_qualification['qualification-' + configuration]['commands']
            )
            and division_qualification['qualification-' + configuration]['core_sha256']
            == division_qualification['fresh-cores-' + configuration]['core_sha256']
            and division_qualification['fresh-cores-' + configuration]['source_sha256']
            == division_delivery['fresh_core_source_sha256']
            and division_qualification['fresh-cores-' + configuration]['commands'][0]['name']
            == 'fresh-build'
            and division_qualification['fresh-cores-' + configuration]['commands'][0]['returncode']
            == 0
            for configuration in ('normal', 'sanitized')
        ),
    )
    sparse_folder = ROOT / 'docs/audit-evidence/sparse-simulation-v1'
    sparse_summary = json.loads((sparse_folder / 'summary.json').read_text())
    with tarfile.open(sparse_folder / 'evidence.tar.gz') as archive:
        sparse_sources = {
            name: hashlib.sha256(archive.extractfile('sources/' + name).read()).hexdigest()
            for name in sparse_summary['source_sha256']
        }
        sparse_records = {
            mode: json.load(archive.extractfile('records/' + mode + '/exit.json'))
            for mode in ('normal', 'sanitized')
        }
        sparse_logs_match = all(
            hashlib.sha256(
                archive.extractfile('records/' + mode + '/' + command['log']).read()
            ).hexdigest()
            == command['log_sha256']
            for mode, record in sparse_records.items()
            for command in record['commands']
        )
    check(
        'Historical sparse159: exact archived sources,88 targeted checks,393 native tests and1509 example checks per configuration',
        sparse_summary['status'] == 'passed-targeted-sparse-and-native-wide-examples'
        and sparse_summary['source_files'] == len(sparse_sources) == 159
        and sparse_sources == sparse_summary['source_sha256']
        and sparse_summary['checks_per_configuration']
        == {'sparse': 88, 'native': 393, 'examples': 1509}
        and sparse_summary['wide_examples_peak_qubits'] == {'wide_integers': 99, 'wide_size': 84}
        and all(
            record['status'] == 'passed-sparse-targeted'
            and record['exit_code'] == 0
            and record['source_sha256'] == sparse_sources
            and all(row['returncode'] == 0 for row in record['commands'])
            for record in sparse_records.values()
        )
        and sparse_records['normal']['commands'][0]['name'] == 'fresh-build'
        and sparse_logs_match,
    )
    rare_born = sparse_summary['rare_born_reproduction']
    check(
        'Sparse159: reused unchanged sanitizer plugin and the actual earlier rare-Born failure retain their identities',
        sparse_records['sanitized']['plugin_reuse'] == sparse_summary['plugin_reuse']
        and sparse_summary['plugin_reuse']['binary_sha256']
        == division_qualification['qualification-sanitized']['core_sha256']['cket_plugin.so']
        and all(
            sparse_sources[name] == sha
            for name, sha in sparse_summary['plugin_reuse']['plugin_inputs_sha256'].items()
        )
        and rare_born['status'] == 'confirmed-rare-born-cutoff-fix'
        and rare_born['records']['previous153']['test_returncode'] == 1
        and rare_born['records']['current159']['test_returncode'] == 0
        and rare_born['records']['previous153']['library_sha256']
        == division_qualification['qualification-normal']['core_sha256']['libcket_quantum.a']
        and rare_born['records']['current159']['library_sha256']
        == sparse_records['normal']['core_sha256']['libcket_quantum.a'],
    )
    shift_folder = ROOT / 'docs/audit-evidence/shift-mux-v1'
    shift_summary = json.loads((shift_folder / 'summary.json').read_text())
    with tarfile.open(shift_folder / 'evidence.tar.gz') as archive:
        shift_sources = {
            name: hashlib.sha256(archive.extractfile('sources/' + name).read()).hexdigest()
            for name in shift_summary['source_sha256']
        }
        shift_records = {
            mode: json.load(archive.extractfile('records/' + mode + '/exit.json'))
            for mode in ('normal', 'sanitized')
        }
        shift_logs_match = all(
            hashlib.sha256(
                archive.extractfile('records/' + mode + '/' + command['log']).read()
            ).hexdigest()
            == command['log_sha256']
            for mode, record in shift_records.items()
            for command in record['commands']
        )
    check(
        'Historical shift161: exact archived sources,71 shift checks,3505 expressions and393 native tests per configuration',
        shift_summary['status'] == 'passed-targeted-shift-multiplexer'
        and shift_summary['source_files'] == len(shift_sources) == 161
        and shift_sources == shift_summary['source_sha256']
        and shift_summary['checks_per_configuration']
        == {'shifts': 71, 'expressions': 3505, 'native': 393}
        and shift_summary['new_complete_sparse_domains_per_configuration'] == 16
        and shift_summary['variable_shift_peak_qubits'] == 105
        and all(
            record['status'] == 'passed-shift-mux-targeted'
            and record['exit_code'] == 0
            and record['source_sha256'] == shift_sources
            and all(row['returncode'] == 0 for row in record['commands'])
            for record in shift_records.values()
        )
        and shift_logs_match,
    )
    check(
        'Shift161: unchanged sanitizer plugin provenance and the real previous compile/resource failure remain explicit',
        shift_records['normal']['commands'][0]['name'] == 'fresh-build'
        and shift_records['sanitized']['plugin_reuse'] == shift_summary['plugin_reuse']
        and shift_summary['plugin_reuse']['binary_sha256']
        == sparse_records['sanitized']['core_sha256']['cket_plugin.so']
        and all(
            shift_sources[n] == sha
            for n, sha in shift_summary['plugin_reuse']['plugin_inputs_sha256'].items()
        )
        and shift_summary['previous_failure']['status']
        == 'confirmed-previous-32bit-shift-resource-limit'
        and shift_summary['previous_failure']['source_sha256']
        == shift_sources['examples/blocks/02/variable_shift.c']
        and shift_summary['previous_failure']['commands'][-1]['name'] == 'compile'
        and shift_summary['previous_failure']['commands'][-1]['returncode'] == 1
        and shift_summary['previous_failure']['previous_core_sha256']['libcket_quantum.a']
        == sparse_records['normal']['core_sha256']['libcket_quantum.a'],
    )
    truth_folder = ROOT / 'docs/audit-evidence/shift-truth-v3'
    truth_summary = json.loads((truth_folder / 'summary.json').read_text())
    truth_native = json.loads((truth_folder / 'native-example.json').read_text())
    with tarfile.open(truth_folder / 'evidence.tar.gz') as archive:
        truth_sources = {
            name: hashlib.sha256(archive.extractfile('sources/' + name).read()).hexdigest()
            for name in truth_summary['source_sha256']
        }
        truth_records = {
            mode: json.load(archive.extractfile('records/' + mode + '/exit.json'))
            for mode in ('normal', 'sanitized')
        }
        truth_logs_match = all(
            hashlib.sha256(
                archive.extractfile('records/' + mode + '/' + command['log']).read()
            ).hexdigest()
            == command['log_sha256']
            for mode, record in truth_records.items()
            for command in record['commands']
        )
    check(
        'Historical truth164: exact archived sources,203 shift checks,3505 expressions,393 native cases and990 divisions per configuration',
        truth_summary['status'] == 'passed-targeted-truth-cast-and-logical-not-correction'
        and truth_summary['source_files'] == len(truth_sources) == 164
        and truth_sources == truth_summary['source_sha256']
        and truth_summary['checks_per_configuration']
        == {'shifts': 203, 'expressions': 3505, 'native': 393, 'division': 990}
        and truth_summary['new_complete_sparse_domains_per_configuration'] == 132
        and truth_summary['example']['peak_qubits'] == 85
        and all(
            record['status'] == 'passed-shift-truth-targeted'
            and record['exit_code'] == 0
            and record['source_sha256'] == truth_sources
            and all(row['returncode'] == 0 for row in record['commands'])
            for record in truth_records.values()
        )
        and truth_logs_match,
    )
    truth_exact = truth_summary['exact_regressions']
    check(
        'Truth164: actual resource failures, numeric wrong-state reproduction, earlier failed candidates and exact native example stay distinct',
        truth_summary['plugin_reuse']['binary_sha256']
        == shift_records['sanitized']['core_sha256']['cket_plugin.so']
        and all(
            truth_sources[n] == sha
            for n, sha in truth_summary['plugin_reuse']['plugin_inputs_sha256'].items()
        )
        and truth_exact['status'] == 'confirmed-three-resource-fixes-and-word-not-correction'
        and len(truth_exact['current_cases'])
        == len(truth_exact['previous_resource_cases']['cases'])
        == 3
        and all(row['returncode'] == 1 for row in truth_exact['previous_resource_cases']['cases'])
        and all(
            row['compile_returncode'] == 0 and all(r['returncode'] == 0 for r in row['resources'])
            for row in truth_exact['current_cases']
        )
        and [
            truth_exact['word_not'][key]['returncode']
            for key in ('previous161', 'candidate164v2', 'current164v3')
        ]
        == [1, 1, 0]
        and truth_exact['word_not']['previous161']['library_sha256']
        == shift_records['normal']['core_sha256']['libcket_quantum.a']
        and truth_exact['word_not']['current164v3']['library_sha256']
        == truth_records['normal']['core_sha256']['libcket_quantum.a']
        and all(
            row['status'] == 'failed-shift-truth-targeted' and row['exit_code'] == 1
            for modes in truth_summary['failed_attempts'].values()
            for row in modes.values()
        )
        and truth_native['status'] == 'passed-32-native-predicate-executions'
        and truth_native['source_sha256'] == truth_sources['examples/blocks/02/shift_predicate.c']
        and all(
            row['compile_returncode'] == 0
            and len(row['runs']) == 16
            and all(r['returncode'] == 0 for r in row['runs'])
            and row['core_sha256'] == truth_records[mode]['core_sha256']
            for mode, row in truth_native['configurations'].items()
        ),
    )
    strings_folder = ROOT / 'docs/audit-evidence/string-library-v1'
    strings_summary = json.loads((strings_folder / 'summary.json').read_text())
    with tarfile.open(strings_folder / 'evidence.tar.gz') as archive:
        strings_sources = {
            name: hashlib.sha256(archive.extractfile('sources/' + name).read()).hexdigest()
            for name in strings_summary['source_sha256']
        }
        strings_logs = {
            name: hashlib.sha256(archive.extractfile('records/' + name).read()).hexdigest()
            for name in ('build.log', 'block06.log')
        }
    check(
        'String168 implementation: exact current/archive sources; seven interfaces remain partial with full qualification deferred',
        strings_summary['status'] == 'built-and-examples-executed-full-qualification-deferred'
        and strings_summary['source_files'] == len(strings_sources) == 168
        and strings_sources == strings_summary['source_sha256']
        and all(
            hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == sha
            for name, sha in strings_sources.items()
        )
        and set(strings_summary['new_partial_interfaces'])
        == {'strspn', 'strcspn', 'memchr', 'strchr', 'strrchr', 'strpbrk', 'strstr'}
        and all(value == 'deferred' for value in strings_summary['qualification'].values()),
    )
    check(
        'String168 delivery: normal build and two native examples are separate from deferred exhaustive tests',
        strings_summary['build']['exit_code'] == strings_summary['delivery']['exit_code'] == 0
        and strings_summary['delivery']['native_executions']
        == strings_summary['delivery']['qasm_exports']
        == strings_summary['delivery']['resource_reports']
        == 2
        and strings_summary['build']['log_sha256'] == strings_logs['build.log']
        and strings_summary['delivery']['log_sha256'] == strings_logs['block06.log'],
    )
    current_capsules = (
        ('controlled-operands-integration', current_integration),
        ('controlled-operands-full', current_full),
        ('coherent-control-full', previous_full),
        ('controlled-operands-v5', operand_v5),
        ('integrated137-remote-ci', integrated_ci),
        ('integrated138-remote-ci', current_ci),
        ('deferred-phase-v6', deferred_phase),
        ('deferred-phase-v6-broad-failure', phase_broad_failure),
        ('strings-integration-v6', string_integration),
        ('integrated144-remote-ci', integrated144_ci),
        ('known-fill-cleanup-v2', fill_integration),
        ('common-remainder-v2', remainder),
        ('selection-views-v5', selection),
        ('common-division-v3', division_delivery),
        ('sparse-simulation-v1', sparse_summary),
        ('shift-mux-v1', shift_summary),
        ('shift-truth-v3', truth_summary),
        ('string-library-v1', strings_summary),
    )
    check(
        'Deferred phase broad regression: the actual failed closure is preserved separately from its running snapshot',
        phase_broad_failure['status'] == 'failed-broad-campaign'
        and phase_broad_failure['exit_code'] == 1
        and phase_broad_failure['closed_commands'] == 57
        and phase_broad_failure['closed_zero_commands'] == 56
        and phase_broad_failure['failed_command'] == 'sanitized-bindings'
        and phase_broad_failure['source_sha256'] == deferred_phase['source_sha256']
        and phase_broad_failure['source_archive_sha256']
        == hashlib.sha256(
            (ROOT / 'docs/audit-evidence/deferred-phase-v6/sources.tar.gz').read_bytes()
        ).hexdigest(),
    )
    check(
        'Current evidence: internal capsule artifacts match their recorded bytes after formatting',
        all(
            hashlib.sha256((ROOT / 'docs/audit-evidence' / folder / name).read_bytes()).hexdigest()
            == expected
            for folder, capsule in current_capsules
            for name, expected in capsule['artifact_sha256'].items()
        ),
    )
    targets = [
        *SOURCES.values(),
        *PDFS.values(),
        *sorted((BASE / 'figures').rglob('*.png')),
        *sorted((BASE / 'figures').rglob('*.svg')),
        *[ROOT / p for p in local_paths],
        *sorted(EVIDENCE.glob('*.log')),
        ROOT / 'docs/audit-evidence/verify.log',
        ROOT / 'docs/audit-evidence/results.json',
        ROOT / 'docs/audit-evidence/synthesis-growth.csv',
        ROOT / 'docs/audit-evidence/broad-source.tar.gz',
        *sorted(p for p in (ROOT / 'docs/audit-evidence/memcmp').iterdir() if p.is_file()),
        *sorted(p for p in (ROOT / 'docs/audit-evidence/strncmp').iterdir() if p.is_file()),
        *sorted(p for p in (ROOT / 'docs/audit-evidence/byte-snapshots').iterdir() if p.is_file()),
        *sorted(
            p for p in (ROOT / 'docs/audit-evidence/memory-expressions').iterdir() if p.is_file()
        ),
        *sorted(p for p in (ROOT / 'docs/audit-evidence/memset').iterdir() if p.is_file()),
        *sorted(p for p in (ROOT / 'docs/audit-evidence/memory-returns').iterdir() if p.is_file()),
        *sorted(
            path
            for folder in (
                'pointer-differences',
                'pointer-differences-maintained',
                'coherent-cpu-conditions',
                'coherent-cpu-conditions-maintained',
                'coherent-memory-integration',
                'coherent-arm-operands',
                'controlled-operands',
                'controlled-operands-maintained',
                'controlled-operands-regression-failure',
                'controlled-operands-v5',
                'integrated137-remote-ci',
                'integrated138-remote-ci',
                'integrated144-remote-ci',
                'known-fill-cleanup-v2',
                'common-remainder-v2',
                'selection-views-v5',
                'common-division-v3',
                'sparse-simulation-v1',
                'shift-mux-v1',
                'shift-truth-v3',
                'string-library-v1',
                'deferred-phase-v6',
                'deferred-phase-v6-broad-failure',
                'guarded-cpu-loads-v3',
                'strings-integration-v6',
                'memset-counter-correction',
                'coherent-control-full',
                'controlled-operands-full',
                'controlled-operands-integration',
            )
            for path in (ROOT / 'docs/audit-evidence' / folder).iterdir()
            if path.is_file()
        ),
        *sorted(
            p
            for p in (ROOT / 'docs/audit-evidence/memory-returns-regression-failure').iterdir()
            if p.is_file()
        ),
        *sorted(
            p
            for p in (ROOT / 'docs/audit-evidence/memory-returns-source-guard').iterdir()
            if p.is_file()
        ),
        *sorted(
            p
            for p in (ROOT / 'docs/audit-evidence/memory-returns-maintained').iterdir()
            if p.is_file()
        ),
        *sorted((ROOT / 'docs/audit-evidence').glob('*-locale-*.log')),
        ROOT / 'docs/re-audit-2026-10-18-deferred-phase.md',
        EVIDENCE / 'synthesis-growth.csv',
        EVIDENCE / 'environment.txt',
        EVIDENCE / 'editorial-toolchain.json',
        EVIDENCE / 'layout-reference.json',
        EVIDENCE / 'manuscript-before-format.md',
        EVIDENCE / 'entanglement.state.json',
        EVIDENCE / 'find.state.json',
        EVIDENCE / 'entanglement.qir.txt',
        EVIDENCE / 'find.qir.txt',
        *[
            BASE / name
            for name in (
                'audit.md',
                'README.md',
                'build_paper.py',
                'audit_paper.py',
                'audit_project.sh',
                'capture_examples.sh',
                'requirements-paper.txt',
                'setup_pdf.sh',
                'inspect_reference.py',
            )
        ],
    ]
    evidence_paths = sorted(
        {
            str(path.relative_to(ROOT))
            for path in targets
            if path.is_relative_to(ROOT / 'docs/audit-evidence')
        }
    )
    attributes = subprocess.run(
        ['git', 'check-attr', '-z', '--stdin', 'text'],
        cwd=ROOT,
        input=b'\0'.join(path.encode() for path in evidence_paths) + b'\0',
        capture_output=True,
        check=True,
    ).stdout.split(b'\0')[:-1]
    check(
        'Evidence byte preservation: Git disables text normalization for every hashed capsule artifact',
        len(attributes) == 3 * len(evidence_paths)
        and all(
            attributes[index * 3] == path.encode()
            and attributes[index * 3 + 1 : index * 3 + 3] == [b'text', b'unset']
            for index, path in enumerate(evidence_paths)
        ),
    )
    manifest = {
        str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in targets
    }
    write_json(EVIDENCE / 'sha256-manifest.json', manifest)
    revision = subprocess.run(
        ['git', 'rev-parse', 'HEAD'], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout.strip()
    dirty = bool(
        subprocess.run(
            ['git', 'status', '--porcelain'], cwd=ROOT, capture_output=True, text=True, check=True
        ).stdout.strip()
    )
    report = {
        'status': 'passed',
        'language': language,
        'checks': len(checks),
        'pages': len(pdf),
        'figures': len(figures),
        'references': len(refs),
        'project_commit': '1ebb2e308e61bed5632798ded5beecf77b6b9a9c',
        'results': checks,
        'audit_checkout': {
            'head': revision,
            'uncommitted_changes': dirty,
            'source_files_in_manifest': len(local_paths),
        },
        'evidence_scope': 'Baseline campaign logs from the fixed revision; editorial and exact examples checked in the current checkout.',
        'visual_review': 'See audit.md: manual review of the latest rendered pages is also required.',
        'external_availability_scope': 'See docs/re-audit-2026-10-14.md: the historical software URL returned HTTP 404 without authentication. PDF/bibliographic checks do not certify public source availability.',
    }
    report_name = 'paper-audit.json' if language == 'es' else 'paper-audit-en.json'
    write_json(EVIDENCE / report_name, report, ensure_ascii=False)
    print(json.dumps({k: v for k, v in report.items() if k != 'results'}, ensure_ascii=False))


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--language', choices=('es', 'en', 'all'), default='all')
    args = parser.parse_args()
    for language in ('es', 'en') if args.language == 'all' else (args.language,):
        main(language)
