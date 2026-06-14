#!/usr/bin/env python3
"""Prepare a YOLO classification dataset from captured class folders."""

from __future__ import annotations

import argparse
import random
import shutil
from pathlib import Path


IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff"}


def image_files(folder: Path) -> list[Path]:
    return sorted(p for p in folder.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_SUFFIXES)


def copy_split(files: list[Path], destination: Path) -> int:
    destination.mkdir(parents=True, exist_ok=True)
    copied = 0
    for source in files:
        shutil.copy2(source, destination / source.name)
        copied += 1
    return copied


def prepare_dataset(source: Path, output: Path, val_ratio: float, seed: int, clean: bool) -> None:
    class_root = source / "classes"
    if not class_root.is_dir():
        raise SystemExit(f"Class folder not found: {class_root}")

    if clean and output.exists():
        shutil.rmtree(output)

    rng = random.Random(seed)
    total_train = 0
    total_val = 0
    class_count = 0
    class_totals: dict[str, int] = {}

    for class_dir in sorted(p for p in class_root.iterdir() if p.is_dir()):
        files = image_files(class_dir)
        if not files:
            continue

        rng.shuffle(files)
        val_count = max(1, round(len(files) * val_ratio)) if len(files) > 1 else 0
        val_files = files[:val_count]
        train_files = files[val_count:]
        if not train_files and val_files:
            train_files.append(val_files.pop())

        class_name = class_dir.name
        class_totals[class_name] = len(files)
        total_train += copy_split(train_files, output / "train" / class_name)
        total_val += copy_split(val_files, output / "val" / class_name)
        class_count += 1

    if class_count == 0:
        raise SystemExit(f"No classes with images found in: {class_root}")

    print(f"Prepared dataset: {output}")
    print(f"Classes: {class_count}")
    print(f"Train images: {total_train}")
    print(f"Val images: {total_val}")
    for class_name, count in class_totals.items():
        print(f"Class {class_name}: {count} images")

    smallest = min(class_totals.values())
    largest = max(class_totals.values())
    if smallest > 0 and largest / smallest > 3:
        print("WARNING: dataset is imbalanced. Add images to smaller classes before trusting accuracy.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Prepare a YOLO classification train/val dataset.")
    parser.add_argument("--source", required=True, type=Path, help="ai/classification folder containing classes/")
    parser.add_argument("--output", required=True, type=Path, help="Output dataset folder")
    parser.add_argument("--val-ratio", type=float, default=0.2, help="Validation split ratio")
    parser.add_argument("--seed", type=int, default=42, help="Deterministic shuffle seed")
    parser.add_argument("--clean", action="store_true", help="Delete output before writing")
    args = parser.parse_args()

    prepare_dataset(args.source, args.output, args.val_ratio, args.seed, args.clean)


if __name__ == "__main__":
    main()
