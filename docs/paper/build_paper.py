#!/usr/bin/env python3
"""Generate original figures and typeset the Spanish and English manuscripts."""
from __future__ import annotations

import csv
import html
import json
import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Circle, Rectangle
from PIL import Image as PILImage
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    SimpleDocTemplate,
    Paragraph,
    Spacer,
    Image,
    Table,
    TableStyle,
    PageBreak,
    KeepTogether,
    Preformatted,
    Flowable,
)

BASE = Path(__file__).resolve().parent
ROOT = BASE.parent.parent
FIGURES = BASE / "figures"
SCRATCH = ROOT / "tmp" / "pdfs"
SOURCES = {"es": BASE / "c_ket_paper.md", "en": BASE / "c_ket_paper_en.md"}
OUTPUTS = {
    "es": ROOT / "output" / "pdf" / "c_ket_paper.pdf",
    "en": ROOT / "output" / "pdf" / "c_ket_paper_en.pdf",
}
INK, TEAL, GOLD = "#000000", "#256b85", "#986d27"
PALETTE = [TEAL, "#6d8395", "#b87820", "#a74d51", "#735e9e", "#18344a"]
LABELS = {
    "add": "Suma",
    "sub": "Resta",
    "mul": "Multiplicación",
    "div": "División",
    "mod": "Resto",
    "lt": "Menor que",
}
ENGLISH_LABELS = {
    "Programa C11": "C11 program",
    "declaraciones quant": "quant declarations",
    "AST, tipos y vidas": "AST, types and lifetimes",
    "IR estática JSON": "Static JSON IR",
    "cket.qir, versión 1": "cket.qir, version 1",
    "emisión": "emission",
    "Ejecutable nativo": "Native executable",
    "control clásico de CPU": "Classical CPU control",
    "Runtime C": "C runtime",
    "instrucciones y argumentos": "Instructions and arguments",
    "Motor cuántico C": "C quantum engine",
    "verificador + síntesis": "Verifier + synthesis",
    "compilación": "compilation",
    "Vector de estado": "State vector",
    "ejecución hasta 24 qubits": "Execution up to 24 qubits",
    "QASM + recursos": "QASM + resources",
    "hasta 128 qubits representados": "Up to 128 represented qubits",
    "ruta nativa": "Native path",
    "ruta estática": "Static path",
    "fase": "phase",
    "Probabilidad conjunta": "Joint probability",
    "Resultado (a, b)": "Outcome (a, b)",
    "0,50": "0.50",
    "Preparación uniforme: 1/16": "Uniform preparation: 1/16",
    "r = 3\nP = 0,9613189697": "r = 3\nP = 0.9613189697",
    "Iteraciones de Grover (r)": "Grover iterations (r)",
    "Probabilidad de éxito ideal": "Ideal success probability",
    "Fuera del simulador local": "Outside the local simulator",
    "Qubits del vector de estado (n)": "State-vector qubits (n)",
    "Memoria del vector (MiB)": "Vector memory (MiB)",
    "Puertas lógicas (escala log)": "Logical gates (log scale)",
    "Pico de qubits físicos representados": "Peak represented qubits",
    "Suma": "Addition",
    "Resta": "Subtraction",
    "Multiplicación": "Multiplication",
    "División": "Division",
    "Resto": "Remainder",
    "Menor que": "Less than",
    "Suma / resta": "Addition / subtraction",
    "Suma / resta / multiplicación": "Addition / subtraction / multiplication",
    "Límite de simulación: 24": "Simulation limit: 24",
    "Ancho de cada entrada (bits)": "Width of each input (bits)",
}


def figure_directory(language):
    return FIGURES if language == "es" else FIGURES / language


def save(fig, name, language):
    # Labels must remain readable when the 9.2-inch drawing is set at 5.5 inches.
    for label in fig.findobj(matplotlib.text.Text):
        if language == "en":
            label.set_text(ENGLISH_LABELS.get(label.get_text(), label.get_text()))
        label.set_fontsize(label.get_fontsize() * 1.4)
    if any(ax.axison for ax in fig.axes):
        fig.tight_layout(pad=0.8)
    directory = figure_directory(language)
    fig.savefig(directory / f"{name}.png", dpi=250, bbox_inches="tight", facecolor="white")
    fig.savefig(directory / f"{name}.svg", bbox_inches="tight", facecolor="white")
    plt.close(fig)


def box(ax, x, y, w, h, title, subtitle="", fill="#eef5f6"):
    ax.add_patch(
        FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.025,rounding_size=0.06",
            facecolor=fill,
            edgecolor=INK,
            linewidth=1.05,
        )
    )
    ax.text(
        x + w / 2,
        y + h * 0.65 if subtitle else y + h / 2,
        title,
        ha="center",
        va="center",
        color=INK,
        fontsize=9.5,
        weight="bold",
    )
    if subtitle:
        ax.text(
            x + w / 2, y + h * 0.27, subtitle, ha="center", va="center", fontsize=7.8, color=INK
        )


def arrow(ax, start, end, label="", offset=(0, 0.12)):
    ax.annotate("", xy=end, xytext=start, arrowprops=dict(arrowstyle="->", color=INK, lw=1.4))
    if label:
        ax.text(
            (start[0] + end[0]) / 2 + offset[0],
            (start[1] + end[1]) / 2 + offset[1],
            label,
            fontsize=8,
            color=INK,
            ha="center",
            va="center",
            bbox=dict(facecolor="white", edgecolor="none", pad=1.2),
        )


def charts(language):
    figure_directory(language).mkdir(parents=True, exist_ok=True)
    serif_dir = times_directory()
    if serif_dir:
        for filename in ("times.ttf", "timesbd.ttf", "timesi.ttf", "timesbi.ttf"):
            matplotlib.font_manager.fontManager.addfont(serif_dir / filename)
    plt.rcParams.update(
        {
            "font.family": "Times New Roman" if serif_dir else "STIXGeneral",
            "mathtext.fontset": "stix",
            "font.size": 12,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "axes.labelcolor": INK,
            "text.color": INK,
            "axes.edgecolor": "#a6b5bd",
            "xtick.color": INK,
            "ytick.color": INK,
            "svg.fonttype": "none",
        }
    )
    with (BASE / "evidence" / "synthesis-growth.csv").open() as stream:
        rows = list(csv.DictReader(stream))

    fig, ax = plt.subplots(figsize=(9.2, 4.3))
    ax.set(xlim=(0, 10), ylim=(0, 5))
    ax.axis("off")
    box(ax, 0.15, 3.75, 2.0, 0.85, "Programa C11", "declaraciones quant")
    box(ax, 3.1, 3.75, 2.7, 0.85, "GCC + plugin", "AST, tipos y vidas")
    box(ax, 7.0, 3.75, 2.7, 0.85, "IR estática JSON", "cket.qir, versión 1")
    arrow(ax, (2.2, 4.17), (3.06, 4.17))
    arrow(ax, (5.83, 4.17), (6.96, 4.17), "emisión")
    box(ax, 0.15, 1.85, 2.0, 0.9, "Ejecutable nativo", "control clásico de CPU")
    box(ax, 3.1, 1.85, 2.7, 0.9, "Runtime C", "instrucciones y argumentos")
    box(ax, 7.0, 1.85, 2.7, 0.9, "Motor cuántico C", "verificador + síntesis")
    arrow(ax, (3.1, 3.7), (2.05, 2.8), "compilación", (-0.45, 0))
    arrow(ax, (2.2, 2.3), (3.06, 2.3))
    arrow(ax, (5.83, 2.3), (6.96, 2.3))
    arrow(ax, (8.35, 3.71), (8.35, 2.79), "backend", (0.6, 0))
    box(ax, 3.1, 0.1, 2.7, 0.9, "Vector de estado", "ejecución hasta 24 qubits")
    box(ax, 7.0, 0.1, 2.7, 0.9, "QASM + recursos", "hasta 128 qubits representados")
    arrow(ax, (7.45, 1.8), (5.35, 1.05), "ruta nativa", (-0.1, -0.08))
    arrow(ax, (8.35, 1.8), (8.35, 1.05), "ruta estática", (0.62, 0))
    save(fig, "fig01_architecture", language)

    fig, ax = plt.subplots(figsize=(9.2, 3.0))
    ax.set(xlim=(0, 10), ylim=(-0.25, 3))
    ax.axis("off")
    for y in (1.0, 2.2):
        ax.plot([0.8, 9.5], [y, y], color=INK, lw=1.3)
    ax.text(0.1, 2.2, r"$|x\rangle$", fontsize=15, va="center")
    ax.text(0.1, 1, r"$|0\rangle$", fontsize=15, va="center")
    box(ax, 1.4, 0.64, 1.75, 1.92, r"$U_f$", "qcompute")
    box(ax, 6.5, 0.64, 1.75, 1.92, r"$U_f^{-1}$", "quncompute")
    box(ax, 4.0, 0.64, 1.4, 0.72, r"$Z$", "fase")
    ax.text(3.6, 1.3, r"$|f(x)\rangle$", fontsize=13, ha="center")
    ax.text(9.15, 0.57, r"$|0\rangle$", fontsize=13, ha="center")
    ax.text(
        5,
        -0.05,
        r"$\sum_x\alpha_x|x\rangle|0\rangle\ \longmapsto\ "
        r"\sum_x(-1)^{f(x)}\alpha_x|x\rangle|0\rangle$",
        ha="center",
        fontsize=14,
    )
    save(fig, "fig02_uncomputation", language)

    fig, axes = plt.subplots(1, 2, figsize=(9.2, 3.05), gridspec_kw={"width_ratios": [1.45, 1]})
    ax = axes[0]
    ax.set(xlim=(0, 6.5), ylim=(0, 3))
    ax.axis("off")
    for y, label in [(2.2, r"a: $|0\rangle$"), (0.9, r"b: $|0\rangle$")]:
        ax.plot([1.1, 6.2], [y, y], color=INK, lw=1.3)
        ax.text(0, y, label, va="center", fontsize=11)
    box(ax, 1.8, 1.86, 0.72, 0.68, "H")
    ax.plot([3.5, 3.5], [0.9, 2.2], color=INK, lw=1.5)
    ax.add_patch(Circle((3.5, 2.2), 0.085, color=INK))
    ax.add_patch(Circle((3.5, 0.9), 0.22, facecolor="white", edgecolor=INK, lw=1.5))
    ax.plot([3.28, 3.72], [0.9, 0.9], color=INK)
    ax.plot([3.5, 3.5], [0.68, 1.12], color=INK)
    for y in (2.2, 0.9):
        box(ax, 4.9, y - 0.34, 0.7, 0.68, "M")
    ax.text(
        3.45, 0.05, r"$|\Phi^+\rangle=(|00\rangle+|11\rangle)/\sqrt{2}$", ha="center", fontsize=13
    )
    ax = axes[1]
    ax.bar(
        ["00", "01", "10", "11"],
        [0.5, 0, 0, 0.5],
        color=[TEAL, "#cbd5da", "#cbd5da", TEAL],
        width=0.65,
    )
    ax.set(ylim=(0, 0.62), ylabel="Probabilidad conjunta", xlabel="Resultado (a, b)")
    for i in (0, 3):
        ax.text(i, 0.515, "0,50", ha="center", fontsize=10)
    ax.grid(axis="y", alpha=0.15)
    ax.set_axisbelow(True)
    fig.tight_layout(w_pad=2)
    save(fig, "fig03_bell", language)

    theta = np.arcsin(np.sqrt(1 / 16))
    r = np.arange(9)
    p = np.sin((2 * r + 1) * theta) ** 2
    fig, ax = plt.subplots(figsize=(9.2, 3.15))
    ax.plot(r, p, "o-", color=TEAL, lw=2, ms=6)
    ax.scatter([3], [p[3]], s=110, color=GOLD, zorder=4)
    ax.axhline(1 / 16, color="#8397a4", ls=":", label="Preparación uniforme: 1/16")
    ax.annotate(
        "r = 3\nP = 0,9613189697",
        (3, p[3]),
        xytext=(4.15, 0.81),
        fontsize=10,
        arrowprops=dict(arrowstyle="->", color=GOLD),
        color=INK,
    )
    ax.set(
        xlabel="Iteraciones de Grover (r)",
        ylabel="Probabilidad de éxito ideal",
        xticks=r,
        ylim=(0, 1.06),
        xlim=(-0.2, 8.2),
    )
    ax.legend(frameon=False, loc="lower right", fontsize=9)
    ax.grid(alpha=0.16)
    fig.tight_layout()
    save(fig, "fig04_grover", language)

    fig, ax = plt.subplots(figsize=(9.2, 3.1))
    n = np.arange(12, 33)
    mem = 16 * 2.0**n / 2**20
    ax.semilogy(n, mem, color=TEAL, lw=2.0)
    ax.axvspan(24, 32, color="#ecf0f2", alpha=0.9)
    ax.axvline(24, color=GOLD, ls="--", lw=1.3)
    ax.text(24.25, 0.13, "Fuera del simulador local", fontsize=9, rotation=0)
    for q, label, dy in [
        (16, "16 qubits: 1 MiB", 22),
        (24, "24 qubits: 256 MiB", 24),
        (32, "32 qubits: 64 GiB", -27),
    ]:
        val = 16 * 2.0**q / 2**20
        ax.scatter([q], [val], color=GOLD, s=36, zorder=4)
        ax.annotate(
            label,
            (q, val),
            xytext=(-10 if q == 32 else 6, dy),
            textcoords="offset points",
            ha="right" if q == 32 else "left",
            fontsize=9,
            bbox=dict(facecolor="white", edgecolor="none", alpha=0.94, pad=1.6),
        )
    ax.set(
        xlabel="Qubits del vector de estado (n)",
        ylabel="Memoria del vector (MiB)",
        xlim=(12, 32.6),
        ylim=(0.025, 200000),
        xticks=[12, 16, 20, 24, 28, 32],
    )
    ax.grid(alpha=0.16)
    fig.tight_layout()
    save(fig, "fig05_memory", language)

    for key, filename, ylabel, log in [
        ("gates", "fig06_gate_growth", "Puertas lógicas (escala log)", True),
        ("peak_qubits", "fig07_workspace", "Pico de qubits físicos representados", False),
    ]:
        fig, ax = plt.subplots(figsize=(9.2, 3.35))
        for i, op in enumerate(LABELS):
            if key == 'gates' and op == 'sub':
                continue
            if key == 'peak_qubits' and op in ('sub', 'mul'):
                continue
            label = LABELS[op]
            if op == 'add':
                label = 'Suma / resta' if key == 'gates' else 'Suma / resta / multiplicación'
            group = [row for row in rows if row["operator"] == op]
            ax.plot(
                [int(row["width"]) for row in group],
                [int(row[key]) for row in group],
                marker=["o", "s", "^", "D", "v", "P"][i],
                color=PALETTE[i],
                ls="--" if op == "sub" else "-",
                lw=1.6,
                ms=5,
                label=label,
                zorder=6 - i,
            )
        if log:
            ax.set_yscale("log")
        else:
            ax.axhline(24, color="#b87820", lw=1.1, ls=":")
            ax.text(15.8, 25.7, "Límite de simulación: 24", ha="right", fontsize=8.5, color=GOLD)
            ax.set_ylim(0, 122)
        ax.set(
            xlabel="Ancho de cada entrada (bits)",
            ylabel=ylabel,
            xticks=[4, 8, 12, 16],
            xlim=(3.5, 16.5),
        )
        ax.legend(ncol=3 if key == 'gates' else 2, fontsize=8.5, frameon=False, loc="upper left")
        ax.grid(alpha=0.16)
        fig.tight_layout()
        save(fig, filename, language)


def times_directory():
    """Use an embedded Times face compatible with the reference's Nimbus Roman."""
    for directory in (Path("C:/Windows/Fonts"), Path("/mnt/c/Windows/Fonts")):
        if all(
            (directory / f).is_file()
            for f in ("times.ttf", "timesbd.ttf", "timesi.ttf", "timesbi.ttf")
        ):
            return directory
    return None


def fonts():
    d = Path(matplotlib.get_data_path()) / "fonts" / "ttf"
    roman = times_directory()
    faces = [
        ("PaperSerif", "times.ttf", "STIXGeneral.ttf"),
        ("PaperSerifBold", "timesbd.ttf", "STIXGeneralBol.ttf"),
        ("PaperSerifItalic", "timesi.ttf", "STIXGeneralItalic.ttf"),
        ("PaperSerifBoldItalic", "timesbi.ttf", "STIXGeneralBolIta.ttf"),
    ]
    for name, filename, fallback in faces:
        pdfmetrics.registerFont(TTFont(name, str(roman / filename if roman else d / fallback)))
    pdfmetrics.registerFont(TTFont("PaperMono", str(d / "DejaVuSansMono.ttf")))
    pdfmetrics.registerFontFamily(
        "PaperSerif",
        normal="PaperSerif",
        bold="PaperSerifBold",
        italic="PaperSerifItalic",
        boldItalic="PaperSerifBoldItalic",
    )


def inline(text, refs=True):
    text = html.escape(text)
    text = re.sub(
        r'\[([^\]]+)\]\((https?://[^\s)]+)\)',
        lambda m: f'<link href="{m[2]}" color="{TEAL}">{m[1]}</link>',
        text,
    )
    text = re.sub(r'`([^`]+)`', r'<font name="PaperMono" size="8.1">\1</font>', text)
    text = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', text)
    text = re.sub(r'\*([^*]+)\*', r'<i>\1</i>', text)
    if refs:
        text = re.sub(
            r'\[(\d+(?:,\s*\d+)*)\]',
            lambda m: '['
            + ', '.join(
                f'<link href="#ref{n.strip()}" color="{TEAL}">{n.strip()}</link>'
                for n in m[1].split(',')
            )
            + ']',
            text,
        )
    return text


def equation(source, index, language):
    plt.rcParams["mathtext.fontset"] = "stix"
    source = source.replace(r"\bmod", r"\,\mathrm{mod}\,")
    fig = plt.figure(figsize=(8.8, 0.42))
    fig.text(0.5, 0.5, f"${source}$", ha="center", va="center", fontsize=11, color=INK)
    path = SCRATCH / language / "equations" / f"eq{index}.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=260, bbox_inches="tight", pad_inches=0.07, facecolor="white")
    plt.close(fig)
    w, h = PILImage.open(path).size
    scale = min(1 / 3.61, 396 / w)
    image = Image(str(path), width=w * scale, height=h * scale)
    image.hAlign = "CENTER"
    return image


class TitleBanner(Flowable):
    """The reference's centered serif title between two horizontal rules."""

    def __init__(self, title, width):
        super().__init__()
        self.title, self.width, self.height = title, width, 66

    def draw(self):
        canvas = self.canv
        canvas.setStrokeColor(colors.black)
        canvas.setLineWidth(2.0)
        canvas.line(0, self.height - 2, self.width, self.height - 2)
        canvas.setFont("PaperSerifBold", 17.2)
        canvas.drawCentredString(self.width / 2, 32, self.title)
        canvas.setLineWidth(0.5)
        canvas.line(0, 14, self.width, 14)


class CodeBlock(Preformatted):
    """Keep examples intact while honoring the introduction's keepWithNext."""

    def split(self, availWidth, availHeight):
        return []


def document(language):
    fonts()
    width = 396  # 5.5 inches, matching the single-column reference.
    normal = ParagraphStyle(
        "normal",
        fontName="PaperSerif",
        fontSize=10,
        leading=11.4,
        textColor=colors.black,
        alignment=TA_JUSTIFY,
        spaceAfter=5,
        rightIndent=2,
        allowWidows=0,
        allowOrphans=0,
        splitLongWords=1,
    )
    styles = {
        "normal": normal,
        "normal_intro": ParagraphStyle("normal_intro", parent=normal, keepWithNext=1),
        "author": ParagraphStyle(
            "author", parent=normal, alignment=TA_CENTER, leading=12, spaceAfter=2
        ),
        "abstract_heading": ParagraphStyle(
            "abstract_heading",
            parent=normal,
            fontName="PaperSerifBold",
            fontSize=12,
            alignment=TA_CENTER,
            leading=14,
            spaceBefore=16,
            spaceAfter=7,
            keepWithNext=1,
        ),
        "abstract": ParagraphStyle(
            "abstract", parent=normal, leftIndent=30, rightIndent=30, spaceAfter=14
        ),
        "h2": ParagraphStyle(
            "h2",
            parent=normal,
            fontName="PaperSerifBold",
            fontSize=12,
            leading=14,
            spaceBefore=10,
            spaceAfter=6,
            alignment=TA_LEFT,
            keepWithNext=1,
        ),
        "h3": ParagraphStyle(
            "h3",
            parent=normal,
            fontName="PaperSerifBold",
            fontSize=10,
            leading=12,
            spaceBefore=8,
            spaceAfter=5,
            alignment=TA_LEFT,
            keepWithNext=1,
        ),
        "caption": ParagraphStyle(
            "caption", parent=normal, fontSize=9.5, leading=10.8, spaceAfter=8
        ),
        "table_caption": ParagraphStyle(
            "table_caption", parent=normal, fontSize=9.5, leading=10.8, spaceBefore=3, spaceAfter=5
        ),
        "cell": ParagraphStyle(
            "cell",
            parent=normal,
            fontSize=9.3,
            leading=10.3,
            spaceAfter=0,
            alignment=TA_LEFT,
            rightIndent=0,
        ),
        "reference": ParagraphStyle(
            "reference",
            parent=normal,
            fontSize=9.5,
            leading=10.8,
            spaceAfter=4,
            alignment=TA_LEFT,
            leftIndent=17,
            firstLineIndent=-17,
        ),
        "code": ParagraphStyle(
            "code",
            fontName="PaperMono",
            fontSize=8,
            leading=9.4,
            textColor=colors.black,
            spaceBefore=3,
            spaceAfter=7,
        ),
    }
    lines = SOURCES[language].read_text(encoding="utf-8").splitlines()
    title = lines[0].removeprefix("# ")
    story, idx, eq = [], 0, 0
    in_authors = in_abstract = False
    pending_table_caption = None
    while idx < len(lines):
        line = lines[idx].strip()
        if not line:
            idx += 1
            continue
        if line == "<!-- authors -->":
            in_authors = True
            idx += 1
            continue
        if line == "<!-- /authors -->":
            in_authors = False
            idx += 1
            continue
        if line == "<!-- page -->":
            story.append(PageBreak())
            idx += 1
            continue
        if line.startswith("<!-- table: "):
            pending_table_caption = line[len("<!-- table: ") : -4].strip()
            idx += 1
            continue
        if line.startswith("<!--"):
            idx += 1
            continue
        if line.startswith("```"):
            idx += 1
            code = []
            while idx < len(lines) and not lines[idx].startswith("```"):
                code.append(lines[idx])
                idx += 1
            story.append(CodeBlock("\n".join(code), styles["code"]))
            idx += 1
            continue
        if line == "$$":
            idx += 1
            latex = []
            while lines[idx].strip() != "$$":
                latex.append(lines[idx])
                idx += 1
            eq += 1
            story.extend([Spacer(1, 2), equation(" ".join(latex), eq, language), Spacer(1, 5)])
            idx += 1
            continue
        if line.startswith("!["):
            match = re.match(r'!\[(.+)\]\((.+)\)', line)
            path = BASE / match[2]
            with PILImage.open(path) as bitmap:
                w, h = bitmap.size
            figure = Image(str(path), width=width, height=width * h / w)
            figure.hAlign = "CENTER"
            block = [figure, Spacer(1, 4), Paragraph(inline(match[1]), styles["caption"])]
            # Group a preceding heading directly with its figure; nesting a
            # KeepTogether can otherwise leave the heading on the prior page.
            while (
                story
                and isinstance(story[-1], Paragraph)
                and getattr(story[-1].style, "keepWithNext", 0)
            ):
                block.insert(0, story.pop())
            story.append(KeepTogether(block))
            idx += 1
            continue
        if line.startswith("|"):
            data = []
            while idx < len(lines) and lines[idx].strip().startswith("|"):
                cells = [c.strip() for c in lines[idx].strip().strip("|").split("|")]
                if not all(re.fullmatch(r'[:\- ]+', c) for c in cells):
                    if not data:
                        cells = ["**" + c + "**" for c in cells]
                    data.append([Paragraph(inline(c), styles["cell"]) for c in cells])
                idx += 1
            n = len(data[0])
            if n == 2:
                widths = [width * 0.46, width * 0.54]
            elif n == 3:
                widths = [width * 0.29, width * 0.28, width * 0.43]
            else:
                widths = [width * 0.28] + [width * 0.72 / (n - 1)] * (n - 1)
            table = Table(data, colWidths=widths, repeatRows=1, hAlign="CENTER")
            table.setStyle(
                TableStyle(
                    [
                        ("LINEABOVE", (0, 0), (-1, 0), 0.8, colors.black),
                        ("LINEBELOW", (0, 0), (-1, 0), 0.5, colors.black),
                        ("LINEBELOW", (0, -1), (-1, -1), 0.8, colors.black),
                        ("VALIGN", (0, 0), (-1, -1), "TOP"),
                        ("LEFTPADDING", (0, 0), (-1, -1), 4),
                        ("RIGHTPADDING", (0, 0), (-1, -1), 4),
                        ("TOPPADDING", (0, 0), (-1, -1), 3),
                        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
                    ]
                )
            )
            block = []
            if pending_table_caption:
                block.append(Paragraph(inline(pending_table_caption), styles["table_caption"]))
            block.extend([table, Spacer(1, 7)])
            story.append(KeepTogether(block))
            pending_table_caption = None
            continue
        if line.startswith("# "):
            story.append(TitleBanner(line[2:], width))
            idx += 1
            continue
        if line.startswith("## "):
            title = line[3:]
            in_abstract = title in ("Resumen", "Abstract")
            style = "abstract_heading" if in_abstract else "h2"
            story.append(Paragraph(inline(title), styles[style]))
            idx += 1
            continue
        if line.startswith("### "):
            story.append(Paragraph(inline(line[4:]), styles["h3"]))
            idx += 1
            continue
        para = [line]
        idx += 1
        while (
            idx < len(lines)
            and lines[idx].strip()
            and not lines[idx].startswith(("#", "|", "```", "$$", "![", "<!--"))
        ):
            para.append(lines[idx].strip())
            idx += 1
        content = " ".join(para)
        reference = re.match(r'^\[(\d+)\]', content)
        if reference:
            content = f'<a name="ref{reference[1]}"/>' + inline(content, refs=False)
            story.append(Paragraph(content, styles["reference"]))
        else:
            style = "author" if in_authors else "abstract" if in_abstract else "normal"
            if style == "normal" and content.rstrip().endswith(":"):
                style = "normal_intro"
            story.append(Paragraph(inline(content), styles[style]))

    output = OUTPUTS[language]
    output.parent.mkdir(parents=True, exist_ok=True)

    def furniture(canvas, doc):
        canvas.saveState()
        canvas.setFillColor(colors.black)
        canvas.setFont("PaperSerif", 10)
        canvas.drawCentredString(letter[0] / 2, 42, str(doc.page))
        canvas.restoreState()

    from reportlab.platypus import BaseDocTemplate, Frame, PageTemplate

    doc = BaseDocTemplate(
        str(output),
        pagesize=letter,
        leftMargin=108,
        rightMargin=108,
        topMargin=72,
        bottomMargin=72,
        title=lines[0].removeprefix("# "),
        author="Ignacio Schwindt",
        subject=(
            "C Ket: arquitectura, evaluación y límites del prototipo"
            if language == "es"
            else "C Ket: architecture, evaluation and prototype limitations"
        ),
        creator="C Ket",
        pageCompression=1,
    )
    frame = Frame(
        108,
        72,
        width,
        648,
        leftPadding=0,
        rightPadding=0,
        topPadding=0,
        bottomPadding=0,
        id="paper",
    )
    doc.addPageTemplates(PageTemplate(id="paper", frames=frame, onPage=furniture))
    doc.build(story)

    import pymupdf as fitz

    pdf = fitz.open(output)
    scratch = SCRATCH / language
    render_dir = scratch / "pages"
    render_dir.mkdir(parents=True, exist_ok=True)
    # Only remove obsolete images owned by this renderer in its fixed directory.
    for obsolete in render_dir.glob("page-*.png"):
        obsolete.unlink()
    thumbs = []
    for page in pdf:
        path = render_dir / f"page-{page.number+1:02d}.png"
        page.get_pixmap(matrix=fitz.Matrix(2, 2), alpha=False).save(path)
        thumb = PILImage.open(path).convert("RGB")
        thumb.thumbnail((306, 396))
        thumbs.append(thumb)
    cols, rows = 4, (len(thumbs) + 3) // 4
    sheet = PILImage.new("RGB", (cols * 326, rows * 426), "#e4e4e4")
    from PIL import ImageDraw

    draw = ImageDraw.Draw(sheet)
    for i, thumb in enumerate(thumbs):
        x, y = (i % cols) * 326 + 10, (i // cols) * 426 + 10
        sheet.paste(thumb, (x, y))
        draw.text((x, y + 398), f"{'Página' if language=='es' else 'Page'} {i+1}", fill=INK)
    sheet.save(scratch / "contact-sheet.png")
    layout = [
        {
            "page": p.number + 1,
            "characters": len(p.get_text()),
            "first": p.get_text()[:90],
            "last": p.get_text()[-120:],
        }
        for p in pdf
    ]
    (scratch / "layout-summary.json").write_text(
        json.dumps(layout, ensure_ascii=False, indent=2) + "\n"
    )
    print(
        json.dumps(
            {
                "pdf": str(output),
                "language": language,
                "pages": len(pdf),
                "figures": 7,
                "rendered": str(render_dir),
            },
            ensure_ascii=False,
        )
    )
    if len(pdf) > 8:
        raise RuntimeError(f"The manuscript exceeds the eight-page limit: {len(pdf)} pages")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--language', choices=('es', 'en', 'all'), default='all')
    parser.add_argument('--pdf-only', action='store_true')
    args = parser.parse_args()
    for language in ('es', 'en') if args.language == 'all' else (args.language,):
        if not args.pdf_only:
            charts(language)
        document(language)
