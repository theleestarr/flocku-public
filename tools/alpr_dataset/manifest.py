from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


MANIFEST_NAME = "dataset_manifest.json"
MANIFEST_FORMAT = 1


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def next_dataset_version(manifest_path: Path, explicit: int | None) -> int:
    if explicit is not None:
        if explicit < 1:
            raise ValueError("dataset_version must be >= 1")
        return explicit
    if not manifest_path.is_file():
        return 1
    try:
        prev = json.loads(manifest_path.read_text(encoding="utf-8"))
        prev_ver = int(prev.get("dataset_version", 0))
    except (json.JSONDecodeError, TypeError, ValueError):
        prev_ver = 0
    return max(1, prev_ver + 1)


def write_dataset_manifest(
    out_dir: Path,
    dataset_version: int,
    states: dict[str, dict[str, Any]],
    *,
    source: str | None = None,
    extra: dict[str, Any] | None = None,
) -> Path:
    index_dir = out_dir / "index"
    index_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = index_dir / MANIFEST_NAME

    state_rows: dict[str, dict[str, Any]] = {}
    for code in sorted(states):
        row = states[code]
        cam_path = out_dir / "cams" / f"{code}.bin"
        entry: dict[str, Any] = {
            "records": int(row.get("alert_objects", 0)),
            "bytes": int(row.get("bytes", 0)),
            "dataset_version": dataset_version,
        }
        if cam_path.is_file():
            entry["sha256"] = _sha256(cam_path)
        state_rows[code] = entry

    payload: dict[str, Any] = {
        "format": MANIFEST_FORMAT,
        "dataset_version": dataset_version,
        "built_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "states": state_rows,
    }
    if source:
        payload["source"] = source
    if extra:
        payload.update(extra)

    manifest_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return manifest_path
