#!/usr/bin/env python3
"""Минимальная защита от случайной публикации приватных данных."""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

FORBIDDEN_BASENAMES = {
    "layoutbeep.ini",
    ".env",
    "id_rsa",
    "id_ed25519",
}
FORBIDDEN_SUFFIXES = {".exe", ".pfx", ".p12", ".key", ".pem"}

PATTERNS: list[tuple[str, re.Pattern[str]]] = [
    ("GitHub token", re.compile(r"\b(?:ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})\b")),
    ("OpenAI/API token", re.compile(r"\bsk-[A-Za-z0-9_-]{20,}\b")),
    ("Google API key", re.compile(r"\bAIza[0-9A-Za-z_-]{30,}\b")),
    ("Slack token", re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{10,}\b")),
    ("Private key", re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----")),
    ("Email", re.compile(r"\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b", re.I)),
    ("Windows user path", re.compile(r"\b[A-Za-z]:\\Users\\[^\\\r\n]+", re.I)),
    ("Unix home path", re.compile(r"(?:^|[\s\"'])/(?:home|Users)/[^\s\"']+")),
    ("Sandbox path", re.compile(r"/mnt/" r"data/|/home/" r"oai/", re.I)),
    ("Private IPv4", re.compile(r"\b(?:10\.\d{1,3}\.\d{1,3}\.\d{1,3}|192\.168\.\d{1,3}\.\d{1,3}|172\.(?:1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})\b")),
]

TEXT_SUFFIXES = {
    ".c", ".h", ".cpp", ".hpp", ".md", ".txt", ".ini", ".yml", ".yaml",
    ".json", ".py", ".bat", ".ps1", ".toml", ".cfg", ".conf", ".gitignore",
}


def tracked_files() -> list[Path]:
    try:
        out = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT)
    except Exception as exc:
        print(f"Не удалось получить список файлов Git: {exc}", file=sys.stderr)
        raise SystemExit(2)
    return [ROOT / p.decode("utf-8") for p in out.split(b"\0") if p]


def is_text_candidate(path: Path) -> bool:
    return path.suffix.lower() in TEXT_SUFFIXES or path.name in {"README", "VERSION", ".gitignore"}


def main() -> int:
    problems: list[str] = []
    files = tracked_files()

    for path in files:
        rel = path.relative_to(ROOT).as_posix()
        low_name = path.name.lower()
        if low_name in FORBIDDEN_BASENAMES:
            problems.append(f"Запрещённый пользовательский/секретный файл: {rel}")
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            problems.append(f"Запрещённый бинарный/секретный файл: {rel}")
        if not is_text_candidate(path):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            try:
                text = path.read_text(encoding="utf-8-sig")
            except UnicodeDecodeError:
                continue
        for label, pattern in PATTERNS:
            m = pattern.search(text)
            if m:
                value = m.group(0)
                if len(value) > 80:
                    value = value[:77] + "..."
                problems.append(f"{label}: {rel}: {value}")

    if problems:
        print("Проверка публичного репозитория НЕ пройдена:")
        for item in problems:
            print(f" - {item}")
        return 1

    print(f"Проверка пройдена: просмотрено {len(files)} отслеживаемых файлов, явных приватных данных не найдено.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
