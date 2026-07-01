#!/usr/bin/env python3
"""Compare two YOLO classification models on a validation folder."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff"}


def collect_images(data: Path) -> list[tuple[Path, str]]:
    images: list[tuple[Path, str]] = []
    for class_dir in sorted(p for p in data.iterdir() if p.is_dir()):
        for image in sorted(p for p in class_dir.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_SUFFIXES):
            images.append((image, class_dir.name))
    return images


def evaluate(model_path: Path, images: list[tuple[Path, str]], device: str) -> dict:
    from ultralytics import YOLO

    model = YOLO(str(model_path))
    correct = 0
    confidence_sum = 0.0
    confusion: dict[str, dict[str, int]] = {}

    for image, expected in images:
        result = model(str(image), device=device, verbose=False)[0]
        probs = result.probs
        top_index = int(probs.top1)
        predicted = str(result.names[top_index])
        confidence = float(probs.top1conf)
        confidence_sum += confidence
        if predicted == expected:
            correct += 1
        confusion.setdefault(expected, {})
        confusion[expected][predicted] = confusion[expected].get(predicted, 0) + 1

    total = len(images)
    return {
        "model": str(model_path),
        "total": total,
        "correct": correct,
        "accuracy": correct / total if total else 0.0,
        "avg_confidence": confidence_sum / total if total else 0.0,
        "confusion": confusion,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Compare two YOLO classification models.")
    parser.add_argument("--old", required=True, type=Path)
    parser.add_argument("--new", required=True, type=Path)
    parser.add_argument("--data", required=True, type=Path, help="Validation folder with class subfolders")
    parser.add_argument("--device", default="0")
    args = parser.parse_args()

    if not args.old.is_file():
        raise SystemExit(f"Old model not found: {args.old}")
    if not args.new.is_file():
        raise SystemExit(f"New model not found: {args.new}")
    if not args.data.is_dir():
        raise SystemExit(f"Validation folder not found: {args.data}")

    import torch

    if args.device != "cpu" and not torch.cuda.is_available():
        raise SystemExit("CUDA GPU not available.")

    images = collect_images(args.data)
    if not images:
        raise SystemExit(f"No validation images found: {args.data}")

    payload = {
        "data": str(args.data),
        "old": evaluate(args.old, images, args.device),
        "new": evaluate(args.new, images, args.device),
    }
    print(json.dumps(payload, ensure_ascii=False))


if __name__ == "__main__":
    main()
