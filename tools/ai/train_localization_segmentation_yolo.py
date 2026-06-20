#!/usr/bin/env python3
"""Train and export a YOLO segmentation model for part localization."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--model", default="yolo11n-seg.pt")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--imgsz", type=int, default=640)
    parser.add_argument("--batch", type=int, default=-1)
    parser.add_argument("--device", default="0")
    parser.add_argument("--project", required=True, type=Path)
    parser.add_argument("--name", default="localization_segmentation")
    parser.add_argument("--export-onnx", action="store_true")
    args = parser.parse_args()

    import torch
    from ultralytics import YOLO

    if args.device != "cpu" and not torch.cuda.is_available():
        raise SystemExit("GPU CUDA non disponibile: training interrotto.")

    model = YOLO(args.model)
    results = model.train(
        data=str(args.data.resolve()),
        epochs=args.epochs,
        imgsz=args.imgsz,
        batch=args.batch,
        device=args.device,
        project=str(args.project.resolve()),
        name=args.name,
    )
    best = Path(results.save_dir) / "weights" / "best.pt"
    print(f"Modello migliore: {best}")
    if args.export_onnx:
        YOLO(str(best)).export(format="onnx", device=args.device)


if __name__ == "__main__":
    main()
