from __future__ import annotations

import gzip
import json
from pathlib import Path
from typing import Any, Iterator

try:
    import orjson as _json  # type: ignore

    def _loads(b: bytes | memoryview) -> Any:
        return _json.loads(b)

except Exception:  # pragma: no cover

    def _loads(b: bytes | memoryview) -> Any:
        if isinstance(b, memoryview):
            b = b.tobytes()
        return json.loads(b.decode("utf-8"))


def read_geojson_bytes(path: Path) -> bytes:
    raw = path.read_bytes()
    if path.suffix == ".gz" or str(path).endswith(".geojson.gz"):
        return gzip.decompress(raw)
    return raw


def iter_geojson_features(path: Path) -> Iterator[dict[str, Any]]:
    doc = _loads(read_geojson_bytes(path))
    if doc.get("type") == "FeatureCollection":
        for feat in doc.get("features", []):
            if isinstance(feat, dict):
                yield feat
    elif doc.get("type") == "Feature":
        yield doc


def pick_coords(feature: dict[str, Any]) -> tuple[float, float] | None:
    geom = feature.get("geometry") or {}
    gtype = geom.get("type")
    coords = geom.get("coordinates")
    if gtype == "Point" and isinstance(coords, (list, tuple)) and len(coords) >= 2:
        return float(coords[1]), float(coords[0])  # lat, lon
    return None
