#!/usr/bin/env python3
"""Prepare a deterministic YOLO segmentation dataset from labeled localization images."""

from __future__ import annotations

import argparse
import random
import shutil
from pathlib import Path


IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff"}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", required=True, type=Path)
    parser.add_argument("--labels", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--val-ratio", type=float, default=0.2)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    pairs: list[tuple[Path, Path]] = []
    for image in sorted(args.raw.iterdir()):
        if not image.is_file() or image.suffix.lower() not in IMAGE_SUFFIXES:
            continue
        label = args.labels / f"{image.stem}.txt"
        if label.is_file() and label.read_text(encoding="utf-8").strip():
            pairs.append((image, label))

    if len(pairs) < 2:
        raise SystemExit("Servono almeno 2 immagini etichettate per preparare train e validation.")
    if args.clean and args.output.exists():
        shutil.rmtree(args.output)

    random.Random(args.seed).shuffle(pairs)
    val_count = max(1, round(len(pairs) * args.val_ratio))
    val_count = min(val_count, len(pairs) - 1)
    splits = {"val": pairs[:val_count], "train": pairs[val_count:]}
    for split, items in splits.items():
        image_dir = args.output / "images" / split
        label_dir = args.output / "labels" / split
        image_dir.mkdir(parents=True, exist_ok=True)
        label_dir.mkdir(parents=True, exist_ok=True)
        for image, label in items:
            shutil.copy2(image, image_dir / image.name)
            shutil.copy2(label, label_dir / label.name)

    yaml = (
        f"path: {args.output.resolve().as_posix()}\n"
        "train: images/train\n"
        "val: images/val\n"
        "names:\n"
        "  0: Pezzo\n"
    )
    (args.output / "data.yaml").write_text(yaml, encoding="utf-8")
    print(f"Dataset localizzazione preparato: {args.output}")
    print(f"Immagini etichettate: {len(pairs)}")
    print(f"Train: {len(splits['train'])}")
    print(f"Validation: {len(splits['val'])}")


if __name__ == "__main__":
    main()
