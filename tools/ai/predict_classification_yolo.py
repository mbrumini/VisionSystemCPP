#!/usr/bin/env python3
"""Run YOLO classification inference on one image."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description="Run YOLO classification inference.")
    parser.add_argument("--model", required=True, type=Path, help="Model file, usually best.pt")
    parser.add_argument("--image", required=True, type=Path, help="Image to classify")
    parser.add_argument("--device", default="0", help="CUDA device. Use cpu only for diagnostics.")
    args = parser.parse_args()

    if not args.model.is_file():
        raise SystemExit(f"Model not found: {args.model}")
    if not args.image.is_file():
        raise SystemExit(f"Image not found: {args.image}")

    import torch
    from ultralytics import YOLO

    if args.device != "cpu" and not torch.cuda.is_available():
        raise SystemExit("CUDA GPU not available. Inference stopped because CPU inference is disabled for this workflow.")

    model = YOLO(str(args.model))
    result = model(str(args.image), device=args.device, verbose=False)[0]
    probs = result.probs
    top_index = int(probs.top1)
    confidence = float(probs.top1conf)
    class_name = result.names[top_index]

    print(f"RESULT class={class_name} confidence={confidence:.4f} index={top_index}")


if __name__ == "__main__":
    main()
