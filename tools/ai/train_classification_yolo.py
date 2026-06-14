#!/usr/bin/env python3
"""Train a YOLO classification model with GPU required."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description="Train YOLO classification using Ultralytics.")
    parser.add_argument("--data", required=True, type=Path, help="Dataset folder with train/ and val/")
    parser.add_argument("--model", default="yolo11s-cls.pt", help="Ultralytics classification model")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--imgsz", type=int, default=224)
    parser.add_argument("--batch", type=int, default=-1)
    parser.add_argument("--device", default="0", help="CUDA device. Use 0 by default.")
    parser.add_argument("--project", type=Path, default=Path("runs/classify"))
    parser.add_argument("--name", default="vision_classification")
    parser.add_argument("--export-onnx", action="store_true")
    args = parser.parse_args()
    args.data = args.data.resolve()
    args.project = args.project.resolve()

    import torch
    from ultralytics import YOLO

    if args.device != "cpu" and not torch.cuda.is_available():
        raise SystemExit("CUDA GPU not available. Training stopped because CPU training is disabled for this workflow.")

    model = YOLO(args.model)
    results = model.train(
        data=str(args.data),
        epochs=args.epochs,
        imgsz=args.imgsz,
        batch=args.batch,
        device=args.device,
        project=str(args.project),
        name=args.name,
    )

    best = Path(results.save_dir) / "weights" / "best.pt"
    print(f"Best model: {best}")

    if args.export_onnx:
        trained = YOLO(str(best))
        trained.export(format="onnx", device=args.device)


if __name__ == "__main__":
    main()
