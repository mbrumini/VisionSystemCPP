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
            reference_mask_path = Path(request["reference_mask"])
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
            classes = result.boxes.cls.detach().cpu().numpy().astype(int)
            piece_indices = np.where(classes == 0)[0]
            if len(piece_indices) == 0:
                emit({
                    "type": "result",
                    "request_id": request_id,
                    "found": False,
                    "camera_id": camera_id,
                    "image": str(image_path),
                    "elapsed_ms": round((time.perf_counter() - infer_started) * 1000.0, 1),
                })
                continue
            best_index = int(piece_indices[np.argmax(confidences[piece_indices])])
            confidence = float(confidences[best_index])
            mask = result.masks.data[best_index].detach().cpu().numpy()
            original_height, original_width = result.orig_shape
            mask = cv2.resize(mask, (original_width, original_height), interpolation=cv2.INTER_NEAREST)
            binary = (mask >= 0.5).astype(np.uint8) * 255
            mask_path.parent.mkdir(parents=True, exist_ok=True)
            if not cv2.imwrite(str(mask_path), binary):
                raise RuntimeError(f"Cannot save mask: {mask_path}")

            reference_indices = np.where(classes == 1)[0]
            reference_found = len(reference_indices) > 0
            reference_confidence = 0.0
            if reference_found:
                combined_reference = np.zeros((original_height, original_width), dtype=np.uint8)
                for reference_index in reference_indices:
                    reference_mask = result.masks.data[int(reference_index)].detach().cpu().numpy()
                    reference_mask = cv2.resize(
                        reference_mask,
                        (original_width, original_height),
                        interpolation=cv2.INTER_NEAREST,
                    )
                    combined_reference = np.maximum(
                        combined_reference,
                        (reference_mask >= 0.5).astype(np.uint8) * 255,
                    )
                reference_mask_path.parent.mkdir(parents=True, exist_ok=True)
                if not cv2.imwrite(str(reference_mask_path), combined_reference):
                    raise RuntimeError(f"Cannot save reference mask: {reference_mask_path}")
                reference_confidence = float(np.max(confidences[reference_indices]))

            emit({
                "type": "result",
                "request_id": request_id,
                "found": True,
                "camera_id": camera_id,
                "image": str(image_path),
                "mask": str(mask_path),
                "reference_found": reference_found,
                "reference_mask": str(reference_mask_path) if reference_found else "",
                "confidence": round(confidence, 6),
                "reference_confidence": round(reference_confidence, 6),
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
