#!/usr/bin/env python3
"""Build and inspect the finite runtime-memory technical note without changing the main papers."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import tarfile

import pymupdf
from reportlab.lib import colors
from reportlab.lib.enums import TA_JUSTIFY
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    SimpleDocTemplate,
    Paragraph,
    Spacer,
    Table,
    TableStyle,
    PageBreak,
    Preformatted,
)

ROOT = Path(__file__).resolve().parents[2]
CAPSULE = ROOT / 'docs/audit-evidence/runtime-memory-v1'
OUTPUT = ROOT / 'output/pdf/cket_memoria_runtime_nota_tecnica.pdf'
QA = ROOT / 'docs/paper/evidence/runtime-memory-note-audit.json'
SOURCE_COMMIT = 'da2ffb31e568026d58b8e5d98aa31929171cfac0'


def main():
    summary = json.loads((CAPSULE / 'summary.json').read_text())
    demo = json.loads((ROOT / 'output/demo/demo-audit.json').read_text())
    assert summary['status'] == 'finite-runtime-memory-qualified'
    assert (
        summary['source_files'],
        summary['full_state_checkpoints'],
        summary['complete_gate_inverses'],
    ) == (221, 900, 270)
    assert demo['source_commit'] == SOURCE_COMMIT and demo['windows_launcher'] == 'passed'
    manifest = hashlib.sha256(
        json.dumps(summary['source_sha256'], sort_keys=True, separators=(',', ':')).encode()
    ).hexdigest()
    with tarfile.open(CAPSULE / 'evidence.tar.gz') as archive:
        source = archive.extractfile('sources/examples/blocks/05/buffer_lengths.c').read().decode()
    assert hashlib.sha256(source.encode()).hexdigest() == demo['source_sha256']
    fonts = {
        'NoteSerif': '/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf',
        'NoteBold': '/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf',
        'NoteMono': '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf',
    }
    for name, path in fonts.items():
        pdfmetrics.registerFont(TTFont(name, path))
    ink, teal, pale = (
        colors.HexColor('#17242d'),
        colors.HexColor('#24677a'),
        colors.HexColor('#eef4f5'),
    )
    body = ParagraphStyle(
        'Body',
        fontName='NoteSerif',
        fontSize=9.3,
        leading=13,
        textColor=ink,
        alignment=TA_JUSTIFY,
        spaceAfter=7,
    )
    heading = ParagraphStyle(
        'Heading',
        fontName='NoteBold',
        fontSize=10.5,
        leading=14,
        textColor=teal,
        spaceBefore=8,
        spaceAfter=5,
    )
    title = ParagraphStyle(
        'Title', fontName='NoteBold', fontSize=20, leading=25, textColor=ink, spaceAfter=9
    )
    small = ParagraphStyle('Small', parent=body, fontSize=8, leading=11)
    mono = ParagraphStyle(
        'Mono', fontName='NoteMono', fontSize=7.5, leading=9.2, textColor=ink, spaceAfter=7
    )
    story = []

    def p(text, style=body):
        story.append(Paragraph(text, style))

    def h(text):
        p(text, heading)

    p('C KET / NOTA TÉCNICA EXPERIMENTAL 01', small)
    p('Memoria cuántica<br/>con longitudes clásicas', title)
    p('Entrega funcional del 24 de octubre de 2026', small)
    p(
        'Un programa nativo combina funciones, arrays enteros y memcpy, memmove y memset con longitudes CPU recibidas durante la ejecución. Las direcciones y longitudes son clásicas; el contenido usa registros cuánticos simulados. El ejemplo observa sus resultados explícitamente y verifica fórmulas C, incluidos los prefijos vacíos.'
    )
    h('1. Modelo y correcciones')
    p(
        'La comprobación cmemory valida límites y solapamiento antes de escribir. Cada palabra seleccionada obtiene una versión y conserva la información necesaria para invertirla. Los dueños públicos permanecen estables entre ramas; las recetas concretas siguen identidades privadas. Los bytes no seleccionados mantienen su valor. El relleno se captura una vez, incluso para longitud cero. No se insertan mediciones ni resets para borrar historia desconocida.'
    )
    p(
        'La extensión conserva valores de base e información: no genera dos estados desconocidos independientes. Se corrigieron una unión que invalidaba un dueño del otro brazo y una lectura del retorno que obtenía un placeholder CPU (0 frente al valor observado 3). La observación explícita corregida lee el registro real; los accesos CPU sin medida y los escapes no comprobados se rechazan.'
    )
    h('2. Evidencia finita reproducible')
    rows = [
        ['Comprobación', 'Resultado'],
        ['Estado complejo conjunto completo', '900 puntos'],
        ['Inversión de todas las puertas', '270 recorridos'],
        ['Preflight antes de mutar', '23 contratos por configuración'],
        ['Núcleo / copia / relleno', '395 / 414 / 940 chequeos'],
        ['Fuentes congeladas / compilador', '221 entradas / GCC 11.4.0'],
    ]
    table = Table(
        [[Paragraph(a, small), Paragraph(b, small)] for a, b in rows],
        colWidths=[267, 236],
        hAlign='LEFT',
    )
    table.setStyle(
        TableStyle(
            [
                ('BACKGROUND', (0, 0), (-1, 0), pale),
                ('LINEABOVE', (0, 0), (-1, 0), 0.8, teal),
                ('LINEBELOW', (0, -1), (-1, -1), 0.6, teal),
                ('TOPPADDING', (0, 0), (-1, -1), 5),
                ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
                ('VALIGN', (0, 0), (-1, -1), 'TOP'),
            ]
        )
    )
    story.append(table)
    story.append(Spacer(1, 7))
    p(
        'Cinco perfiles prueban copia, movimientos en ambos sentidos, relleno que lee su destino y bytes parciales: longitudes 0/1/2 e iteraciones 0/1/3. Los 600 puntos normales (O0/O2) y 300 ASan/UBSan (O1) usan ambas políticas de síntesis. Cada estado se compara con fórmulas independientes y con evolución independiente de puertas. Las ejecuciones nativas y los rechazos se contabilizan aparte; los recuentos no prueban estados arbitrarios.',
        small,
    )
    p(
        'Identidad del código: '
        + SOURCE_COMMIT[:12]
        + '.<br/>Manifest SHA-256: '
        + manifest[:24]
        + '...<br/>Cápsula: docs/audit-evidence/runtime-memory-v1.',
        small,
    )
    story.append(PageBreak())
    p('Programa completo y uso', title)
    p(
        'Fuente ejecutada: examples/blocks/05/buffer_lengths.c. La distribución incluye el ejecutable Linux x86-64, su fuente, licencia, hashes y un lanzador Windows con WSL.',
        small,
    )
    story.append(Preformatted(source, mono))
    h('3. Ejecutar la entrega')
    story.append(
        Preformatted(
            'Windows, desde la raíz del repositorio:\n  output\\demo\\run_memory_demo.cmd\n\nLinux, tras extraer el paquete de output/demo:\n  CKET_SIMULATOR=sparse CKET_SEED=7 ./bin/buffer_lengths 2',
            mono,
        )
    )
    p(
        'El lanzador probado usa stdin para transferir el paquete a /tmp de WSL, sin montar Windows, y ejecuta las tres longitudes. Estas fueron sus salidas reales; cada ejecución devuelve 0 al coincidir con las fórmulas:',
        small,
    )
    for run in demo['native_runs']:
        story.append(Preformatted(run['output'], mono))
    h('4. Alcance')
    p(
        'Direcciones comprobadas, hasta 64 bytes por operación y 128 cables de representación; simulación densa de 24 cables y modo disperso acotado. Faltan direcciones/ABI generales, más flujos de propiedad, cadenas sobre heap y flotantes cuánticos. C cotidiano continúa en desarrollo. Esta evidencia usa un simulador y no demuestra QPU física ni aceleración. Ciberseguridad permanece clásica.',
        small,
    )
    p(
        'Fuentes: [1] programa anterior y commit indicado; [2] summary.json y evidence.tar.gz de runtime-memory-v1; [3] docs/c11-runtime-memory.md. Esta nota agrega evidencia actual. Los manuscritos históricos en español e inglés conservan su revisión y resultados originales.',
        small,
    )
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)

    def page(canvas, doc):
        canvas.setStrokeColor(teal)
        canvas.setLineWidth(0.5)
        canvas.line(46, 29, A4[0] - 46, 29)
        canvas.setFont('NoteSerif', 7)
        canvas.setFillColor(ink)
        canvas.drawString(46, 18, 'C Ket - memoria con longitud CPU - 24 octubre 2026')
        canvas.drawRightString(A4[0] - 46, 18, str(doc.page))

    doc = SimpleDocTemplate(
        str(OUTPUT),
        pagesize=A4,
        rightMargin=46,
        leftMargin=46,
        topMargin=37,
        bottomMargin=41,
        title='Memoria cuántica con longitudes clásicas en C Ket',
        author='Proyecto C Ket',
    )
    doc.build(story, onFirstPage=page, onLaterPages=page)
    pdf = pymupdf.open(OUTPUT)
    assert len(pdf) == 2, len(pdf)
    text = '\n'.join(page.get_text() for page in pdf)
    required = [
        '900',
        '270',
        '221',
        '11.4.0',
        SOURCE_COMMIT[:12],
        'buffer_lengths',
        'run_memory_demo.cmd',
        'C cotidiano',
    ]
    assert all(token in text for token in required)
    assert '\ufffd' not in text and '\u25a0' not in text
    preview = ROOT / 'tmp/pdfs/runtime-memory-note-v1'
    preview.mkdir(parents=True, exist_ok=True)
    images = []
    for i, page in enumerate(pdf, 1):
        for block in page.get_text('blocks'):
            x0, y0, x1, y1 = block[:4]
            assert x0 >= 35 and x1 <= A4[0] - 35 and y0 >= 20 and y1 <= A4[1] - 10, block
        image = preview / f'page-{i}.png'
        page.get_pixmap(matrix=pymupdf.Matrix(1.5, 1.5), alpha=False).save(image)
        images.append(image.relative_to(ROOT).as_posix())
    font_names = sorted({f[3] for page in pdf for f in page.get_fonts()})
    assert any('DejaVu' in name for name in font_names)
    QA.write_text(
        json.dumps(
            dict(
                status='text-and-layout-passed',
                visual_review='pending',
                pages=2,
                source_commit=SOURCE_COMMIT,
                source_files=221,
                source_manifest_sha256=manifest,
                pdf_sha256=hashlib.sha256(OUTPUT.read_bytes()).hexdigest(),
                markdown_sha256=hashlib.sha256(
                    (Path(__file__).parent / 'runtime_memory_note.es.md').read_bytes()
                ).hexdigest(),
                builder_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                embedded_fonts=font_names,
                font_sha256={
                    n: hashlib.sha256(Path(p).read_bytes()).hexdigest() for n, p in fonts.items()
                },
                metrics={
                    k: summary[k]
                    for k in ('full_state_checkpoints', 'complete_gate_inverses', 'source_files')
                },
                rendered_pages=images,
                scope='Finite technical note for the demonstrated runtime-memory delivery; historical papers retain their original revision.',
            ),
            indent=2,
        )
        + '\n'
    )
    print(
        json.dumps(dict(pages=2, text_checks='passed', pdf=str(OUTPUT), previews=images)),
        flush=True,
    )


if __name__ == '__main__':
    main()
