#!/usr/bin/env python3
"""Persistent YOLO segmentation worker for part localization."""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import cv2
import numpy as np


def emit(payload: dict) -> None:
    print(json.dumps(payload, ensure_ascii=False), flush=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", required=True, type=Path)
    parser.add_argument("--device", default="0")
    args = parser.parse_args()

    if not args.model.is_file():
        emit({"type": "error", "message": f"Model not found: {args.model}"})
        raise SystemExit(2)

    import torch
    from ultralytics import YOLO

    if args.device != "cpu" and not torch.cuda.is_available():
        emit({"type": "error", "message": "GPU CUDA non disponibile."})
        raise SystemExit(3)

    started = time.perf_counter()
    model = YOLO(str(args.model))
    emit({
        "type": "ready",
        "model": str(args.model),
        "load_ms": round((time.perf_counter() - started) * 1000.0, 1),
    })

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
            request_id = str(request.get("request_id", ""))
            image_path = Path(request["image"])
            mask_path = Path(request["mask"])
            camera_id = str(request.get("camera_id", ""))
            if not image_path.is_file():
                raise FileNotFoundError(f"Image not found: {image_path}")

            infer_started = time.perf_counter()
            result = model(str(image_path), device=args.device, verbose=False)[0]
            if result.masks is None or len(result.masks.data) == 0:
                emit({
                    "type": "result",
                    "request_id": request_id,
                    "found": False,
                    "camera_id": camera_id,
                    "image": str(image_path),
                    "elapsed_ms": round((time.perf_counter() - infer_started) * 1000.0, 1),
                })
                continue

            confidences = result.boxes.conf.detach().cpu().numpy()
            best_index = int(np.argmax(confidences))
            confidence = float(confidences[best_index])
            mask = result.masks.data[best_index].detach().cpu().numpy()
            original_height, original_width = result.orig_shape
            mask = cv2.resize(mask, (original_width, original_height), interpolation=cv2.INTER_NEAREST)
            binary = (mask >= 0.5).astype(np.uint8) * 255
            mask_path.parent.mkdir(parents=True, exist_ok=True)
            if not cv2.imwrite(str(mask_path), binary):
                raise RuntimeError(f"Cannot save mask: {mask_path}")

            emit({
                "type": "result",
                "request_id": request_id,
                "found": True,
                "camera_id": camera_id,
                "image": str(image_path),
                "mask": str(mask_path),
                "confidence": round(confidence, 6),
                "elapsed_ms": round((time.perf_counter() - infer_started) * 1000.0, 1),
            })
        except Exception as exc:
            emit({
                "type": "error",
                "request_id": locals().get("request_id", ""),
                "message": str(exc),
            })


if __name__ == "__main__":
    main()
