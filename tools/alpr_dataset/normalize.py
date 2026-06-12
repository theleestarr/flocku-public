from __future__ import annotations

from dataclasses import dataclass
from typing import Any


@dataclass
class RawCamera:
    lat: float
    lon: float
    brand: str | None
    operator: str | None
    source_id: str | None
    source_type: str | None
    timestamp: str | None


def _get_str(props: dict[str, Any], *keys: str) -> str | None:
    for k in keys:
        v = props.get(k)
        if v is None:
            continue
        if isinstance(v, str) and v.strip():
            return v.strip()
        if isinstance(v, (int, float)):
            return str(v)
    return None


def normalize_feature(feature: dict[str, Any]) -> RawCamera | None:
    props = feature.get("properties") or {}
    if not isinstance(props, dict):
        props = {}
    geom = feature.get("geometry") or {}
    coords = geom.get("coordinates")
    if geom.get("type") != "Point" or not isinstance(coords, (list, tuple)) or len(coords) < 2:
        return None
    lon, lat = float(coords[0]), float(coords[1])
    return RawCamera(
        lat=lat,
        lon=lon,
        brand=_get_str(props, "brand", "make", "vendor"),
        operator=_get_str(props, "operator", "owner"),
        source_id=_get_str(props, "id", "source_id", "camera_id"),
        source_type=_get_str(props, "source", "source_type"),
        timestamp=_get_str(props, "timestamp", "updated", "last_seen"),
    )


def classify_camera_type(cam: RawCamera) -> int:
    hay = " ".join(
        x.lower()
        for x in (cam.brand, cam.operator, cam.source_type, cam.source_id or "")
        if isinstance(x, str)
    )
    if "flock" in hay:
        return 1  # TYPE_FLOCK
    if "alpr" in hay or "lpr" in hay or "plate" in hay:
        return 2  # TYPE_OTHER_ALPR
    return 0  # TYPE_UNKNOWN_ALPR
