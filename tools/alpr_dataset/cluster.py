from __future__ import annotations

import math
from collections import defaultdict
from dataclasses import dataclass
from typing import Iterable

from . import constants as C


@dataclass(frozen=True)
class CamPoint:
    lat: float
    lon: float
    cam_type: int


@dataclass
class AlertSpec:
    lat: float
    lon: float
    radius_ft: int
    kind: int
    cam_type: int
    count: int
    coverage: int


def _haversine_ft(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    r = 20902231.0  # mean Earth radius in feet (matches firmware geo_math)
    p = math.pi / 180.0
    a1, a2 = lat1 * p, lat2 * p
    dlat = (lat2 - lat1) * p
    dlon = (lon2 - lon1) * p
    x = math.sin(dlat / 2) ** 2 + math.cos(a1) * math.cos(a2) * math.sin(dlon / 2) ** 2
    c = 2 * math.atan2(math.sqrt(x), math.sqrt(max(0.0, 1.0 - x)))
    return r * c


def _bearing_deg(lat0: float, lon0: float, lat: float, lon: float) -> float:
    p = math.pi / 180.0
    y = math.sin((lon - lon0) * p) * math.cos(lat * p)
    x = math.cos(lat0 * p) * math.sin(lat * p) - math.sin(lat0 * p) * math.cos(lat * p) * math.cos((lon - lon0) * p)
    return (math.degrees(math.atan2(y, x)) + 360.0) % 360.0


def _bucket(bearing_deg: float) -> str:
    b = bearing_deg % 360.0
    if b >= 315 or b < 45:
        return "N"
    if b < 135:
        return "E"
    if b < 225:
        return "S"
    return "W"


def _uf_find(parent: list[int], a: int) -> int:
    while parent[a] != a:
        parent[a] = parent[parent[a]]
        a = parent[a]
    return a


def _uf_union(parent: list[int], a: int, b: int) -> None:
    ra, rb = _uf_find(parent, a), _uf_find(parent, b)
    if ra != rb:
        parent[rb] = ra


def cluster_cameras(points: Iterable[CamPoint], cluster_radius_ft: float) -> list[AlertSpec]:
    pts = list(points)
    n = len(pts)
    parent = list(range(n))

    for i in range(n):
        for j in range(i + 1, n):
            if _haversine_ft(pts[i].lat, pts[i].lon, pts[j].lat, pts[j].lon) <= cluster_radius_ft:
                _uf_union(parent, i, j)

    comps: dict[int, list[int]] = defaultdict(list)
    for i in range(n):
        comps[_uf_find(parent, i)].append(i)

    alerts: list[AlertSpec] = []
    for _, members in comps.items():
        if len(members) < 3:
            for k in members:
                alerts.append(
                    AlertSpec(
                        lat=pts[k].lat,
                        lon=pts[k].lon,
                        radius_ft=120,
                        kind=C.KIND_CAMERA_POINT,
                        cam_type=pts[k].cam_type,
                        count=1,
                        coverage=C.COVER_AHEAD,
                    )
                )
            continue

        c_lat = sum(pts[k].lat for k in members) / len(members)
        c_lon = sum(pts[k].lon for k in members) / len(members)
        mx_ft = max(_haversine_ft(c_lat, c_lon, pts[k].lat, pts[k].lon) for k in members)
        radius_ft = int(max(120.0, min(2000.0, mx_ft + 50.0)))

        buckets: set[str] = set()
        for k in members:
            buckets.add(_bucket(_bearing_deg(c_lat, c_lon, pts[k].lat, pts[k].lon)))

        types = {pts[k].cam_type for k in members}
        cam_type = C.TYPE_MIXED if len(types) > 1 else next(iter(types))

        if len(buckets) >= 3:
            kind = C.KIND_INTERSECTION_ZONE
            coverage = C.COVER_NORTH | C.COVER_EAST | C.COVER_SOUTH | C.COVER_WEST
        elif len(buckets) == 2:
            kind = C.KIND_INTERSECTION_ZONE
            coverage = C.COVER_LEFT | C.COVER_RIGHT | C.COVER_AHEAD
        else:
            kind = C.KIND_CAMERA_CLUSTER
            coverage = C.COVER_AHEAD

        alerts.append(
            AlertSpec(
                lat=c_lat,
                lon=c_lon,
                radius_ft=radius_ft,
                kind=kind,
                cam_type=cam_type,
                count=len(members),
                coverage=coverage,
            )
        )

    return alerts
