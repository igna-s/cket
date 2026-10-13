#!/usr/bin/env python3
"""Format repository-owned sources, or check them without modifying files."""

from __future__ import annotations

import argparse
import ast
from collections import Counter
import json
import re
import subprocess
import sys
from pathlib import Path

import black
import mdformat

ROOT = Path(__file__).resolve().parent.parent
CLANG = ROOT / ".deps/format-venv/bin/clang-format"
SHFMT = ROOT / ".deps/format-tools/shfmt"
C_EXTENSIONS = {".c", ".cpp", ".h", ".inc"}
PUBLICATION_SOURCES = {
    "docs/paper/c_ket_paper.md",
    "docs/paper/c_ket_paper_en.md",
}
FENCE = re.compile(r"(^```([^\n]*)\n)(.*?)(^```\s*$)", re.M | re.S)
C_TOKEN = re.compile(
    r"(?P<space>\s+)|(?P<comment>/\*.*?\*/|//[^\n]*)"
    r"|(?:u8|u|U|L)?\"(?:\\.|[^\"\\])*\""
    r"|(?:u|U|L)?'(?:\\.|[^'\\])*'"
    r"|[A-Za-z_][A-Za-z_0-9]*"
    r"|(?:[0-9]|\.[0-9])(?:[A-Za-z_0-9.]|[eEpP][+-])*"
    r"|>>=|<<=|\.\.\.|->\*|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\|"
    r"|\+=|-=|\*=|/=|%=|&=|\^=|\|=|##|::|\.\*|[^\s]",
    re.S,
)


def command(tool: Path, arguments: list[str], text: str) -> str:
    result = subprocess.run(
        [str(tool), *arguments],
        input=text,
        text=True,
        capture_output=True,
        cwd=ROOT,
        check=True,
    )
    return result.stdout


def c_tokens(text: str) -> tuple[list[str], list[tuple[str, bool]]]:
    """Check spelling/order, directive boundaries and macro call syntax.

    This is a formatting guard, not a substitute for compiler/runtime tests.
    Strings remain literal tokens; comment and ordinary whitespace changes
    are allowed. Preprocessor newlines and function-like macro declarations
    remain significant.
    """
    text = re.sub(r"\\\r?\n", "", text)
    tokens = []
    line_start = True
    directive = False
    for match in C_TOKEN.finditer(text):
        value = match.group()
        if match.lastgroup == "space":
            if "\n" in value:
                if directive:
                    tokens.append("<end-directive>")
                line_start = True
                directive = False
            continue
        if match.lastgroup == "comment":
            continue
        if line_start and value == "#":
            directive = True
        line_start = False
        tokens.append(value)
    if directive:
        tokens.append("<end-directive>")
    macros = [
        (match.group(1), match.group(2) == "(")
        for match in re.finditer(r"^\s*#\s*define\s+([A-Za-z_]\w*)(.)?", text, re.M)
    ]
    return tokens, macros


def format_c(text: str, path: Path) -> str:
    formatted = command(CLANG, ["--style=file", f"--assume-filename={path}"], text)
    if c_tokens(text) != c_tokens(formatted):
        raise ValueError(f"C token or preprocessor change: {path.relative_to(ROOT)}")
    return formatted


def format_shell(text: str, path: Path) -> str:
    return command(SHFMT, ["-ln", "bash", "-i", "4", "-ci", "-filename", str(path)], text)


def format_fence(match: re.Match[str]) -> str:
    opening, language, body, closing = match.groups()
    language = language.strip()
    if language in {"c", "cpp", "c++"}:
        body = format_c(body, ROOT / ("snippet.cpp" if language != "c" else "snippet.c"))
    elif language in {"bash", "sh", "shell"}:
        body = format_shell(body, ROOT / "snippet.sh")
    elif language == "json":
        try:
            body = json.dumps(json.loads(body), ensure_ascii=False, indent=2) + "\n"
        except json.JSONDecodeError:
            # Some contracts intentionally show fragments or placeholders.
            pass
    return opening + body + closing


def format_text(path: Path, text: str) -> str:
    relative = path.relative_to(ROOT).as_posix()
    if path.suffix in C_EXTENSIONS:
        return format_c(text, path)
    if path.suffix == ".py":
        mode = black.Mode(
            line_length=100,
            target_versions={black.TargetVersion.PY310},
            string_normalization=False,
        )
        try:
            formatted = black.format_file_contents(text, fast=False, mode=mode)
        except black.NothingChanged:
            return text
        if ast.dump(ast.parse(text)) != ast.dump(ast.parse(formatted)):
            raise ValueError(f"Python AST change: {relative}")
        return formatted
    if path.suffix == ".sh" or relative == "cket":
        return format_shell(text, path)
    if path.suffix == ".md":
        if relative in PUBLICATION_SOURCES:
            # The manuscripts are tied to audited eight-page PDFs. Preserve
            # their prose and code layout; their PDF builder owns typesetting.
            return text.rstrip("\n") + "\n"
        formatted = mdformat.text(text, options={"wrap": 100}, extensions={"gfm"})
        formatted = FENCE.sub(format_fence, formatted)

        # GFM parses unescaped pipes as table separators, including in code
        # spans. A formatter must not silently discard extra cells/contracts.
        def content(source: str) -> tuple[Counter, Counter]:
            source = re.sub(r"(?m)^\s*\d+[.)]\s+", "", source)
            words = Counter(re.findall(r"\w+", source))
            code = Counter(
                span.replace(r"\|", "|").strip()
                for span in re.findall(r"(?<![\\`])`(?!`)([^`\n]*)`(?!`)", source)
            )
            return words, code

        original_words, original_code = content(text)
        updated_words, updated_code = content(formatted)
        if original_words - updated_words or original_code - updated_code:
            raise ValueError(f"Markdown content loss: {relative}")
        return formatted
    if path.suffix == ".json":
        return json.dumps(json.loads(text), ensure_ascii=False, indent=2) + "\n"
    # YAML/configuration whitespace is already structural. Only normalize
    # line endings, trailing spaces and the final newline.
    return "\n".join(line.rstrip() for line in text.splitlines()).rstrip("\n") + "\n"


def repository_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=ROOT,
        capture_output=True,
        check=True,
    )
    files = set()
    for name in result.stdout.decode().split("\0"):
        if (
            not name
            or name.startswith("docs/paper/evidence/")
            or (name.startswith("docs/audit-evidence/") and not name.endswith(".md"))
        ):
            # Historical evidence and generated manifests keep their original
            # bytes and hashes; successful audits regenerate current reports.
            continue
        path = ROOT / name
        if path.is_file() and (
            path.suffix in C_EXTENSIONS | {".py", ".sh", ".md", ".json", ".yml", ".yaml", ".txt"}
            or name in {"cket", ".clang-format", ".editorconfig", ".gitignore", ".gitattributes"}
        ):
            files.add(path)
    return sorted(files)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Report changes without writing them.")
    arguments = parser.parse_args()
    files = repository_files()
    changes = []
    for path in files:
        original = path.read_bytes()
        formatted = format_text(path, original.decode("utf-8").replace("\r\n", "\n"))
        updated = formatted.encode("utf-8")
        if original != updated:
            changes.append(path.relative_to(ROOT).as_posix())
            if not arguments.check:
                path.write_bytes(updated)
    if arguments.check and changes:
        print("Formatting required:\n" + "\n".join(changes))
        return 1
    action = "Checked" if arguments.check else "Formatted"
    print(f"{action} {len(files)} repository files; {len(changes)} changed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
