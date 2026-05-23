#!/usr/bin/env python3
import argparse
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import unicodedata


DEFAULT_MODEL = "vision-help"
DEFAULT_DOCS_DIR = Path("docs/help/it")
TOKEN_RE = re.compile(r"[a-z0-9_]+")
ANSI_RE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]|\x1b\][^\x07]*(?:\x07|\x1b\\)")
SPINNER_RE = re.compile(r"^[\s\u2800-\u28ff]+$")

STOPWORDS = {
    "a", "ad", "al", "alla", "alle", "allo", "anche", "che", "chi", "ci",
    "ciao", "con", "da", "dei", "del", "della", "devo", "defo", "di", "e", "ed", "fare", "faccio", "gli", "ha", "i",
    "il", "in", "io", "la", "le", "lo", "ma", "mi", "ne", "nel", "non",
    "o", "per", "piu", "puo", "se", "si", "su", "sul", "tra", "un",
    "una", "uno", "uso", "vedi", "vedo",
}


def normalize(text):
    text = unicodedata.normalize("NFKD", text.lower())
    text = "".join(ch for ch in text if not unicodedata.combining(ch))
    return text


def tokenize(text):
    return [tok for tok in TOKEN_RE.findall(normalize(text)) if tok not in STOPWORDS and len(tok) > 1]


def expand_query_tokens(query, tokens):
    normalized = normalize(query)
    expanded = list(tokens)

    has_measure = any(token.startswith("misur") or token in {"quota", "quote"} for token in tokens)
    has_negative = any(word in normalized for word in [" non ", " manca", " manc", " assente", " sparisce"])
    if has_measure and has_negative:
        expanded.extend(["misura", "disponibile", "geometria", "sorgente", "mancante", "quota"])

    if any(word in normalized for word in ["lunghezza", "larghezza", "altezza", "quota lineare"]):
        expanded.extend([
            "lunghezza", "larghezza", "altezza", "distanza", "quota", "lineare",
            "punto", "linea", "bordo", "profilo", "superficie", "riferimento"
        ])

    if "edge" in tokens and has_measure:
        expanded.extend(["edge", "geometria", "trovata", "misura", "risultati", "frame"])

    if "luce" in tokens or "illuminazione" in tokens:
        expanded.extend(["illuminazione", "contrasto", "esposizione", "gain"])

    if "diametr" in normalized and "superficie" in normalized:
        expanded.extend([
            "diametro", "superficie", "cerchio", "foro", "luce", "diretta",
            "diffusa", "coassiale", "anulare", "edge"
        ])

    if "misur" in normalized and "superfic" in normalized:
        expanded.extend([
            "misure", "superficiali", "faccia", "foro", "sede", "tasca",
            "luce", "diretta", "geometria", "cerchio", "linea",
            "camera", "candidata", "profilo"
        ])

    if "camera" in tokens or "telecamera" in tokens or "tc" in tokens:
        expanded.extend(["camera", "candidata", "profilo", "misura", "superficie", "dimensionale"])

    if "ross" in normalized or "esclusion" in normalized or "mascher" in normalized:
        expanded.extend(["maschera", "rossa", "esclusione", "localizzazione", "ignora"])

    return expanded


def read_documents(root):
    docs = []
    for path in sorted(root.rglob("*.txt")):
        if path.name.lower() in {"index.txt", "manifest.txt"}:
            continue
        try:
            content = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            content = path.read_text(encoding="latin-1")
        tokens = tokenize(path.stem.replace("_", " ") + "\n" + content)
        if tokens:
            docs.append({"path": path, "content": content.strip(), "tokens": tokens})
    return docs


def score_documents(query, docs):
    query_tokens = expand_query_tokens(query, tokenize(query))
    if not query_tokens:
        return []

    query_counts = {}
    for token in query_tokens:
        query_counts[token] = query_counts.get(token, 0) + 1

    doc_freq = {}
    for doc in docs:
        for token in set(doc["tokens"]):
            doc_freq[token] = doc_freq.get(token, 0) + 1

    scored = []
    total_docs = max(1, len(docs))
    for doc in docs:
        doc_counts = {}
        for token in doc["tokens"]:
            doc_counts[token] = doc_counts.get(token, 0) + 1

        score = 0.0
        title = normalize(doc["path"].stem.replace("_", " "))
        for token, query_count in query_counts.items():
            if token not in doc_counts:
                continue
            idf = math.log((1 + total_docs) / (1 + doc_freq.get(token, 0))) + 1
            title_boost = 2.2 if token in title else 1.0
            score += query_count * (1 + math.log(doc_counts[token])) * idf * title_boost

        if score > 0:
            scored.append((score, doc))

    return sorted(scored, key=lambda item: item[0], reverse=True)


def clean_terminal_output(text):
    text = ANSI_RE.sub("", text)
    lines = []
    for line in text.splitlines():
        line = line.replace("\r", "").rstrip()
        if not line or SPINNER_RE.match(line):
            continue
        lines.append(line)
    return "\n".join(lines).strip()


def trim_content(content, max_chars):
    content = re.sub(r"\n{3,}", "\n\n", content.strip())
    if len(content) <= max_chars:
        return content
    return content[:max_chars].rsplit("\n", 1)[0].strip() + "\n..."


def build_prompt(question, selected_docs):
    context_blocks = []
    for index, doc in enumerate(selected_docs, start=1):
        rel_path = doc["path"].as_posix()
        content = trim_content(doc["content"], 1400)
        context_blocks.append(f"[Documento {index}: {rel_path}]\n{content}")

    context = "\n\n".join(context_blocks)
    return (
        "Usa solo il contesto qui sotto per rispondere alla domanda.\n"
        "I documenti sono ordinati per pertinenza: dai piu peso al Documento 1.\n"
        "Rispondi al tema della domanda, senza trasformarla in una checklist generica.\n"
        "Se manca una informazione, dillo e indica quale controllo pratico fare.\n"
        "Alla fine aggiungi una riga \"Fonti:\" con i nomi dei file usati.\n\n"
        f"Contesto:\n{context}\n\n"
        f"Domanda:\n{question}"
    )



def run_ollama(model, prompt):
    try:
        result = subprocess.run(
            ["ollama", "run", "--nowordwrap", model],
            input=prompt,
            text=True,
            capture_output=True,
            check=False,
            encoding="utf-8",
        )
    except FileNotFoundError:
        print("Ollama non e nel PATH. Installa o avvia Ollama prima di usare questo script.", file=sys.stderr)
        return 127

    stderr = clean_terminal_output(result.stderr)
    stdout = clean_terminal_output(result.stdout)
    if stderr:
        print(stderr, file=sys.stderr)
    if stdout:
        print(stdout)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(description="Interroga il manuale VisionSystemCPP con Ollama.")
    parser.add_argument("question", nargs="+", help="Domanda da fare al manuale.")
    parser.add_argument("--model", default=os.environ.get("VISION_HELP_MODEL", DEFAULT_MODEL))
    parser.add_argument("--docs", type=Path, default=DEFAULT_DOCS_DIR)
    parser.add_argument("--top", type=int, default=3)
    parser.add_argument("--min-score-ratio", type=float, default=0.25)
    parser.add_argument("--dry-run", action="store_true", help="Mostra prompt e fonti senza chiamare Ollama.")
    args = parser.parse_args()

    question = " ".join(args.question)
    docs_root = args.docs
    if not docs_root.exists():
        print(f"Cartella documenti non trovata: {docs_root}", file=sys.stderr)
        return 2

    docs = read_documents(docs_root)
    scored = score_documents(question, docs)
    best_score = scored[0][0] if scored else 0.0
    selected = [
        doc
        for score, doc in scored[: max(1, args.top)]
        if best_score == 0.0 or score >= best_score * args.min_score_ratio
    ]

    if not selected:
        print("Nessun documento pertinente trovato. Prova con termini piu specifici.", file=sys.stderr)
        return 1

    print("Fonti selezionate:")
    for score, doc in scored[: len(selected)]:
        print(f"- {doc['path'].as_posix()} ({score:.2f})")
    print()

    prompt = build_prompt(question, selected)
    if args.dry_run:
        print(prompt)
        return 0

    return run_ollama(args.model, prompt)


if __name__ == "__main__":
    raise SystemExit(main())
