#!/usr/bin/env python3
"""Read the user's layout reference; save typography data and QA previews."""
import collections
import json
from pathlib import Path
import pymupdf as fitz

ROOT = Path(__file__).resolve().parents[2]
TMP = ROOT / "tmp" / "pdfs"
doc = fitz.open(TMP / "reference.pdf")
sizes = collections.Counter()
for page in doc:
    for block in page.get_text("dict")["blocks"]:
        for line in block.get("lines", []):
            for span in line["spans"]:
                sizes[(span["font"], round(span["size"], 2))] += len(span["text"])
report = {
    "pages": len(doc),
    "page_size": list(doc[0].rect),
    "fonts": [
        {"font": key[0], "size": key[1], "characters": count}
        for key, count in sizes.most_common(14)
    ],
    "first_page_blocks": [
        {
            "bbox": list(b["bbox"]),
            "text": " ".join(s["text"] for l in b.get("lines", []) for s in l["spans"]),
            "fonts": sorted(
                {(s["font"], round(s["size"], 2)) for l in b.get("lines", []) for s in l["spans"]}
            ),
        }
        for b in doc[0].get_text("dict")["blocks"]
        if b["type"] == 0
    ],
}
(TMP / "reference-layout.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
for number in (0, 1, 4, 7):
    doc[number].get_pixmap(matrix=fitz.Matrix(1.7, 1.7)).save(TMP / f"reference-{number+1:02d}.png")
print(json.dumps({key: report[key] for key in ("pages", "page_size", "fonts")}, indent=2))
