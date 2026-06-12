from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import geopandas as gpd
from shapely.geometry import Point
from shapely.geometry.base import BaseGeometry


@dataclass(frozen=True)
class StateRow:
    code: str
    polygon4326: BaseGeometry
    buffered4326: BaseGeometry


def load_states(states_path: Path, buffer_miles: float) -> list[StateRow]:
    gdf = gpd.read_file(states_path)
    if "geometry" not in gdf.columns:
        raise ValueError("states file must contain geometry")

    gdf = gdf.to_crs(5070)
    buf_m = float(buffer_miles) * 1609.344
    buffered = gdf.geometry.buffer(buf_m)

    gdf_buf = gpd.GeoDataFrame(gdf.drop(columns=["geometry"]), geometry=buffered, crs=gdf.crs)
    gdf_buf = gdf_buf.to_crs(4326)

    gdf_wgs = gdf.to_crs(4326)

    code_col = None
    for cand in ("STUSPS", "postal", "state", "STATEFP", "abbrev"):
        if cand in gdf.columns:
            code_col = cand
            break
    if code_col is None:
        raise ValueError("Could not find a state code column (expected STUSPS/postal/state).")

    rows: list[StateRow] = []
    for i in range(len(gdf_wgs)):
        code = str(gdf_wgs.iloc[i][code_col]).strip()
        if len(code) != 2:
            continue
        rows.append(
            StateRow(
                code=code.upper(),
                polygon4326=gdf_wgs.geometry.iloc[i],
                buffered4326=gdf_buf.geometry.iloc[i],
            )
        )
    return rows


def assign_cameras_to_states(
    cameras: list[tuple[float, float, object]],
    states: list[StateRow],
) -> dict[str, list[object]]:
    """Assign each camera payload to all state buffered polygons that contain it."""
    out: dict[str, list[object]] = {s.code: [] for s in states}
    for lat, lon, payload in cameras:
        p = Point(lon, lat)
        for st in states:
            if st.buffered4326.covers(p):
                out[st.code].append(payload)
    return out


def state_bbox_e7(st: StateRow) -> tuple[str, int, int, int, int]:
    minx, miny, maxx, maxy = st.buffered4326.bounds  # lon/lat
    return (
        st.code,
        int(round(miny * 1e7)),
        int(round(maxy * 1e7)),
        int(round(minx * 1e7)),
        int(round(maxx * 1e7)),
    )


def state_centroid_e7(st: StateRow) -> tuple[int, int]:
    """WGS84 centroid of the unbuffered state polygon (lat e7, lon e7)."""
    c = st.polygon4326.centroid
    return int(round(c.y * 1e7)), int(round(c.x * 1e7))


def state_index_record_e7(st: StateRow) -> tuple[str, int, int, int, int, int, int]:
    """Buffered bbox + true-state centroid for /index/states.bin."""
    code, mnlat, mxlat, mnlon, mxlon = state_bbox_e7(st)
    clat, clon = state_centroid_e7(st)
    return code, mnlat, mxlat, mnlon, mxlon, clat, clon
