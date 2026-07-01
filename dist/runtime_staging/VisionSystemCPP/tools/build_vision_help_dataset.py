#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import re


DEFAULT_DOCS_DIR = Path("docs/help/it/functions")
DEFAULT_OUTPUT = Path("ollama/vision-help/vision_help_dataset.jsonl")


def parse_doc(path):
    text = path.read_text(encoding="utf-8").strip()
    lines = text.splitlines()
    title = path.stem.replace("_", " ")
    area = ""
    keywords = ""
    body_start = 0

    for index, line in enumerate(lines[:8]):
        lower = line.lower()
        if lower.startswith("titolo:"):
            title = line.split(":", 1)[1].strip()
            body_start = max(body_start, index + 1)
        elif lower.startswith("area:"):
            area = line.split(":", 1)[1].strip()
            body_start = max(body_start, index + 1)
        elif lower.startswith("parole chiave:"):
            keywords = line.split(":", 1)[1].strip()
            body_start = max(body_start, index + 1)

    body = "\n".join(lines[body_start:]).strip()
    return {
        "path": path,
        "title": title,
        "area": area,
        "keywords": keywords,
        "body": body,
        "text": text,
    }


def compact(text):
    text = re.sub(r"\n{3,}", "\n\n", text.strip())
    return text


def make_examples(doc):
    title = doc["title"]
    source = doc["path"].name
    answer = compact(doc["text"])
    examples = [
        {
            "instruction": f"Spiega la funzione: {title}",
            "input": "",
            "output": f"{answer}\n\nFonte: {source}",
        },
        {
            "instruction": f"Quando devo usare {title} in VisionSystemCPP?",
            "input": "",
            "output": f"{answer}\n\nFonte: {source}",
        },
    ]

    if doc["area"]:
        examples.append(
            {
                "instruction": f"Dammi un consiglio pratico su {title}.",
                "input": f"Area: {doc['area']}",
                "output": f"{answer}\n\nFonte: {source}",
            }
        )

    if doc["keywords"]:
        examples.append(
            {
                "instruction": f"Quali controlli fare per: {doc['keywords']}?",
                "input": "",
                "output": f"{answer}\n\nFonte: {source}",
            }
        )

    return examples


def main():
    parser = argparse.ArgumentParser(description="Genera un dataset JSONL dai file help VisionSystemCPP.")
    parser.add_argument("--docs", type=Path, default=DEFAULT_DOCS_DIR)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    docs = [parse_doc(path) for path in sorted(args.docs.glob("*.txt"))]
    args.output.parent.mkdir(parents=True, exist_ok=True)

    count = 0
    with args.output.open("w", encoding="utf-8", newline="\n") as handle:
        for doc in docs:
            for example in make_examples(doc):
                handle.write(json.dumps(example, ensure_ascii=False) + "\n")
                count += 1

    print(f"Dataset scritto: {args.output}")
    print(f"Documenti: {len(docs)}")
    print(f"Esempi: {count}")


if __name__ == "__main__":
    raise SystemExit(main())
