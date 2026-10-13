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
                'docs/c11-inplace.md',
                'docs/c11-memcopy.md',
                'docs/c11-division.md',
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
                'docs/audit-evidence/README.md',
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
