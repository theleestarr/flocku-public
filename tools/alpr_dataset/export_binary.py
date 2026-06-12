from __future__ import annotations

import struct
from pathlib import Path

from . import constants as C
from .cluster import AlertSpec


HEADER = struct.Struct("<4s B 2s I I")
RECORD = struct.Struct("<ii H B B B B")


def write_camera_bin(
    path: Path,
    state_code: str,
    alerts: list[AlertSpec],
    *,
    dataset_version: int,
) -> int:
    path.parent.mkdir(parents=True, exist_ok=True)
    code = state_code.upper().encode("ascii")
    if len(code) != 2:
        raise ValueError("state_code must be two letters")
    if dataset_version < 1:
        raise ValueError("dataset_version must be >= 1")
    payload = HEADER.pack(C.MAGIC, C.VERSION, code, len(alerts), int(dataset_version))
    for a in alerts:
        payload += RECORD.pack(
            int(round(a.lat * 1e7)),
            int(round(a.lon * 1e7)),
            int(a.radius_ft) & 0xFFFF,
            int(a.kind) & 0xFF,
            int(a.cam_type) & 0xFF,
            int(a.count) & 0xFF,
            int(a.coverage) & 0xFF,
        )
    path.write_bytes(payload)
    return len(payload)


def write_states_bin(path: Path, boxes: list[tuple[str, int, int, int, int, int, int]]) -> int:
    """boxes: (ST, min_lat_e7, max_lat_e7, min_lon_e7, max_lon_e7, cent_lat_e7, cent_lon_e7)"""
    path.parent.mkdir(parents=True, exist_ok=True)
    buf = struct.pack("<I", len(boxes))
    for st, mnlat, mxlat, mnlon, mxlon, clat, clon in boxes:
        code = st.upper().encode("ascii")
        if len(code) != 2:
            raise ValueError(st)
        buf += struct.pack("<2siiiiii", code, mnlat, mxlat, mnlon, mxlon, clat, clon)
    path.write_bytes(buf)
    return len(buf)
