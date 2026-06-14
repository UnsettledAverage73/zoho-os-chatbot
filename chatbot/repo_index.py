from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import math
import os
import re
from collections import Counter, defaultdict
from functools import lru_cache
from typing import Iterable


TEXT_EXTENSIONS = {
    ".c",
    ".h",
    ".asm",
    ".s",
    ".rs",
    ".py",
    ".md",
    ".mdx",
    ".txt",
    ".toml",
    ".yml",
    ".yaml",
    ".json",
    ".cfg",
    ".mk",
    ".make",
    ".ld",
    ".tex",
    ".sh",
    ".html",
    ".css",
    ".js",
    ".jsx",
}

SKIP_PARTS = {
    ".git",
    ".docusaurus",
    "node_modules",
    "build",
    "dist",
    "target",
    "isodir",
    "ramdisk_root",
}

SKIP_SUFFIXES = {
    ".png",
    ".jpg",
    ".jpeg",
    ".gif",
    ".mp4",
    ".zip",
    ".pdf",
    ".o",
    ".a",
    ".so",
    ".elf",
    ".iso",
    ".img",
    ".d",
}

TOKEN_RE = re.compile(r"[A-Za-z0-9_./\\-]+")
MENTION_RE = re.compile(r"@([A-Za-z0-9_./\\-]+)")
FILE_TAG_RE = re.compile(r"@file[:\s]+([A-Za-z0-9_./\\-]+)")


@dataclass(frozen=True)
class Chunk:
    path: str
    start_line: int
    end_line: int
    text: str
    tokens: Counter[str]

    @property
    def label(self) -> str:
        return f"{self.path}:{self.start_line}-{self.end_line}"


@dataclass
class SearchResult:
    chunk: Chunk
    score: float


def _is_text_path(path: Path) -> bool:
    if any(part in SKIP_PARTS for part in path.parts):
        return False
    if path.suffix.lower() in SKIP_SUFFIXES:
        return False
    if path.suffix.lower() in TEXT_EXTENSIONS:
        return True
    return path.name in {"Makefile", "README", "LICENSE"}


def _read_text(path: Path) -> str | None:
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if b"\x00" in data[:4096]:
        return None
    for encoding in ("utf-8", "utf-8-sig", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return None


def _tokenize(text: str) -> Counter[str]:
    tokens = [token.lower() for token in TOKEN_RE.findall(text)]
    return Counter(tokens)


def _chunk_lines(lines: list[str], max_lines: int = 80, overlap: int = 16) -> Iterable[tuple[int, int, str]]:
    if not lines:
        return
    start = 0
    total = len(lines)
    while start < total:
        end = min(total, start + max_lines)
        chunk_lines = lines[start:end]
        yield start + 1, end, "".join(chunk_lines)
        if end >= total:
            break
        start = max(end - overlap, start + 1)


def _normalize_query(query: str) -> str:
    return query.replace("\\", "/").strip()


def _path_key(path: Path) -> str:
    return path.as_posix()


class RepoIndex:
    def __init__(self, root: Path):
        self.root = root
        self.chunks: list[Chunk] = []
        self.file_paths: list[str] = []
        self.file_summaries: dict[str, str] = {}
        self._build()

    def _build(self) -> None:
        files: list[Path] = []
        for path in self.root.rglob("*"):
            if path.is_dir():
                continue
            if not _is_text_path(path):
                continue
            files.append(path)

        for path in sorted(files):
            text = _read_text(path)
            if text is None:
                continue

            rel = _path_key(path.relative_to(self.root))
            self.file_paths.append(rel)
            self.file_summaries[rel] = self._summarize_file(rel, text)

            lines = text.splitlines(keepends=True)
            for start_line, end_line, chunk_text in _chunk_lines(lines):
                tokens = _tokenize(chunk_text)
                if not tokens:
                    continue
                self.chunks.append(
                    Chunk(
                        path=rel,
                        start_line=start_line,
                        end_line=end_line,
                        text=chunk_text,
                        tokens=tokens,
                    )
                )

        self._build_idf()

    def _build_idf(self) -> None:
        self.doc_freq: defaultdict[str, int] = defaultdict(int)
        for chunk in self.chunks:
            for token in chunk.tokens:
                self.doc_freq[token] += 1
        self.total_docs = max(1, len(self.chunks))

    def _summarize_file(self, rel_path: str, text: str) -> str:
        lower = rel_path.lower()
        if lower == "src/kernel/main.c":
            return "Kernel entry point. Initializes memory, interrupts, devices, networking, and the shell."
        if lower == "src/boot/main.asm":
            return "32-bit bootstrap. Verifies Multiboot2, checks CPU features, builds paging, and jumps to long mode."
        if lower == "src/boot/long_mode_init.asm":
            return "64-bit entry stub. Receives the Multiboot2 handoff and calls kmain()."
        if lower == "src/boot/header.asm":
            return "Multiboot2 header consumed by GRUB."
        if lower == "src/kernel/shell.c":
            return "Interactive shell. Handles commands, output, and terminal rendering."
        if lower == "docs-site/docs/overview.mdx":
            return "High-level OS architecture and recommended reading order."

        first_heading = ""
        for line in text.splitlines():
            stripped = line.strip()
            if stripped.startswith("#"):
                first_heading = stripped.lstrip("#").strip()
                break
        if first_heading:
            return first_heading
        return "Source file in the Zoho OS repository."

    @lru_cache(maxsize=256)
    def resolve_mentions(self, query: str) -> tuple[str, ...]:
        normalized = _normalize_query(query)
        mentions = []
        for raw in FILE_TAG_RE.findall(normalized):
            candidate = raw.strip().rstrip(".,;:!?")
            candidate = candidate.lstrip("./")
            resolved = self._resolve_path(candidate)
            if resolved:
                mentions.append(resolved)
        for raw in MENTION_RE.findall(normalized):
            candidate = raw.strip().rstrip(".,;:!?")
            if candidate == "file":
                continue
            if candidate.startswith("file/"):
                candidate = candidate[5:]
            candidate = candidate.lstrip("./")
            resolved = self._resolve_path(candidate)
            if resolved:
                mentions.append(resolved)
        return tuple(dict.fromkeys(mentions))

    def _resolve_path(self, candidate: str) -> str | None:
        candidate_norm = candidate.replace("\\", "/").lstrip("/")
        if candidate_norm in self.file_summaries:
            return candidate_norm

        matches = [path for path in self.file_paths if path.endswith(candidate_norm)]
        if len(matches) == 1:
            return matches[0]
        if len(matches) > 1:
            return min(matches, key=len)

        basename = Path(candidate_norm).name
        basename_matches = [path for path in self.file_paths if Path(path).name == basename]
        if len(basename_matches) == 1:
            return basename_matches[0]
        if len(basename_matches) > 1:
            return min(basename_matches, key=len)
        return None

    def search(self, query: str, *, top_k: int = 6, pinned_paths: Iterable[str] = (), mode: str = "general") -> list[SearchResult]:
        normalized = _normalize_query(query)
        query_tokens = _tokenize(normalized)
        query_terms = set(query_tokens)
        pinned = {path for path in pinned_paths if path in self.file_summaries}
        mention_paths = set(self.resolve_mentions(query))
        selected_paths = pinned | mention_paths

        results: list[SearchResult] = []
        for chunk in self.chunks:
            score = self._score_chunk(chunk, query_terms, query_tokens, selected_paths, mode)
            if score <= 0:
                continue
            results.append(SearchResult(chunk=chunk, score=score))

        results.sort(key=lambda item: item.score, reverse=True)
        return results[:top_k]

    def _score_chunk(
        self,
        chunk: Chunk,
        query_terms: set[str],
        query_tokens: Counter[str],
        selected_paths: set[str],
        mode: str,
    ) -> float:
        if not query_terms and not selected_paths:
            return 0.0

        score = 0.0
        for term, qtf in query_tokens.items():
            if term not in chunk.tokens:
                continue
            df = self.doc_freq.get(term, 1)
            idf = math.log((self.total_docs + 1) / (df + 1)) + 1.0
            score += (1 + math.log(qtf)) * idf * (1 + math.log(1 + chunk.tokens[term]))

        if chunk.path in selected_paths:
            score += 6.0

        path_tokens = set(TOKEN_RE.findall(chunk.path.lower()))
        if query_terms & path_tokens:
            score += 1.5

        if mode == "semantic":
            score *= 1.2
        elif mode == "code":
            if chunk.path.endswith((".c", ".h", ".asm", ".rs", ".py")):
                score *= 1.15
        elif mode == "docs":
            if chunk.path.endswith((".md", ".mdx", ".txt")):
                score *= 1.15

        return score

    def format_context(self, query: str, *, pinned_paths: Iterable[str] = (), mode: str = "general", top_k: int = 6) -> str:
        results = self.search(query, top_k=top_k, pinned_paths=pinned_paths, mode=mode)
        lines = [
            "Repository snapshot:",
            f"- root: {self.root}",
            f"- mode: {mode}",
            f"- query: {query.strip()}",
            "",
            "Relevant files:",
        ]

        seen: set[str] = set()
        for result in results:
            chunk = result.chunk
            if chunk.path not in seen:
                seen.add(chunk.path)
                lines.append(f"- {chunk.path} ({self.file_summaries.get(chunk.path, 'source file')})")

        if not seen:
            lines.append("- no direct matches found; answer from the project overview and general repo structure")

        lines.append("")
        lines.append("Snippets:")
        for result in results:
            chunk = result.chunk
            lines.append(f"### {chunk.label}")
            snippet = chunk.text.strip()
            lines.append(snippet[:2200] if len(snippet) > 2200 else snippet)
            lines.append("")

        return "\n".join(lines).strip()

    def explain_file(self, path: str) -> str:
        path = self._resolve_path(path) or path
        summary = self.file_summaries.get(path, "Source file in the Zoho OS repository.")
        return summary


def load_repo_index(root: str | Path) -> RepoIndex:
    return RepoIndex(Path(root))
