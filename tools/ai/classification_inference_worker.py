#!/usr/bin/env python3
"""Persistent YOLO classification inference worker.

Input protocol: one JSON object per stdin line:
  {"image": "path/to/image.png"}

Output protocol: one JSON object per stdout line.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path


def emit(payload: dict) -> None:
    print(json.dumps(payload, ensure_ascii=False), flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Persistent YOLO classification inference worker.")
    parser.add_argument("--model", required=True, type=Path, help="Model file, usually best.pt")
    parser.add_argument("--device", default="0", help="CUDA device. Use cpu only for diagnostics.")
    args = parser.parse_args()

    if not args.model.is_file():
        emit({"type": "error", "message": f"Model not found: {args.model}"})
        raise SystemExit(2)

    import torch
    from ultralytics import YOLO

    if args.device != "cpu" and not torch.cuda.is_available():
        emit({
            "type": "error",
            "message": "CUDA GPU not available. Inference stopped because CPU inference is disabled for this workflow.",
        })
        raise SystemExit(3)

    load_start = time.perf_counter()
    model = YOLO(str(args.model))
    emit({
        "type": "ready",
        "model": str(args.model),
        "load_ms": round((time.perf_counter() - load_start) * 1000.0, 1),
    })

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
            image = Path(request["image"])
            if not image.is_file():
                emit({"type": "error", "message": f"Image not found: {image}"})
                continue

            infer_start = time.perf_counter()
            result = model(str(image), device=args.device, verbose=False)[0]
            probs = result.probs
            top_index = int(probs.top1)
            confidence = float(probs.top1conf)
            class_name = result.names[top_index]
            emit({
                "type": "result",
                "image": str(image),
                "class": class_name,
                "confidence": round(confidence, 6),
                "index": top_index,
                "elapsed_ms": round((time.perf_counter() - infer_start) * 1000.0, 1),
            })
        except Exception as exc:
            emit({"type": "error", "message": str(exc)})


if __name__ == "__main__":
    main()
