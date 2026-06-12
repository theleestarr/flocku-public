#!/usr/bin/env python3
"""
Download ALPR nodes from DeFlock's public CDN (same tile index as maps.deflock.org).

Source: https://cdn.deflock.me/regions/index.json — tiles at regions/{lat}/{lon}.json

This does not scrape deflock.org HTML; it uses the same JSON tiles their map loads.

ODbL: Underlying data traces to OpenStreetMap; respect OSM license if you redistribute.

Example:
  ./fetch_deflock_geojson.py --eastern-wa --flock-only -o /tmp/eastern_wa_flock.geojson
  ./fetch_deflock_geojson.py --us --flock-only -o /tmp/us_flock.geojson
"""
from __future__ import annotations

import argparse
import json
import math
import sys
import urllib.request
from pathlib import Path
from typing import Any


UA = "flocku-fetch-deflock/1.0 (+local dataset build; https://github.com)"


def _get_json(url: str) -> Any:
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=120) as resp:
        return json.loads(resp.read().decode("utf-8"))


def _tile_keys_for_bbox(
    regions: list[str],
    south: float,
    north: float,
    west: float,
    east: float,
    tile_size: float,
) -> list[str]:
    """Same iteration as DeFlock webapp stores/tiles.ts (viewport tiles)."""
    reg_set = set(regions)
    keys: list[str] = []
    lat = math.floor(south / tile_size) * tile_size
    max_lat = math.ceil(north / tile_size) * tile_size
    while lat <= max_lat:
        lng = math.floor(west / tile_size) * tile_size
        max_lng = math.ceil(east / tile_size) * tile_size
        while lng <= max_lng:
            kl = f"{int(lat)}/{int(lng)}"
            if kl in reg_set:
                keys.append(kl)
            lng += tile_size
        lat += tile_size
    return keys


def _is_flock(tags: dict[str, Any]) -> bool:
    if not tags:
        return False
    hay = " ".join(str(v).lower() for v in tags.values() if v is not None)
    return "flock" in hay


def main() -> None:
    ap = argparse.ArgumentParser(description="Fetch DeFlock CDN tiles → GeoJSON Points.")
    ap.add_argument(
        "--eastern-wa",
        action="store_true",
        help="Clip to Eastern WA rough bbox (lon [-121,-116.9], lat [45.5,49]).",
    )
    ap.add_argument(
        "--us",
        action="store_true",
        help="Clip to continental US + Alaska + Hawaii (matches DeFlock CDN tile set; ~12 tiles).",
    )
    ap.add_argument("--south", type=float, default=None)
    ap.add_argument("--north", type=float, default=None)
    ap.add_argument("--west", type=float, default=None)
    ap.add_argument("--east", type=float, default=None)
    ap.add_argument(
        "--flock-only",
        action="store_true",
        help="Keep only nodes whose tags mention Flock (vendor filter).",
    )
    ap.add_argument("-o", "--output", type=Path, required=True, help="Output .geojson path")
    args = ap.parse_args()

    if args.eastern_wa and args.us:
        ap.error("use only one of --eastern-wa and --us")
    if args.eastern_wa:
        south, north, west, east = 45.5, 49.0, -121.0, -116.9
    elif args.us:
        # WGS84 envelope covering CONUS, AK, HI; aligned with cdn.deflock.me/regions index tiles.
        south, north, west, east = 18.5, 72.0, -179.0, -66.5
    else:
        if None in (args.south, args.north, args.west, args.east):
            ap.error("use --eastern-wa, --us, or set --south --north --west --east")
        south, north, west, east = args.south, args.north, args.west, args.east

    base = "https://cdn.deflock.me/regions"
    idx = _get_json(f"{base}/index.json")
    tile_size = float(idx["tile_size_degrees"])
    regions_list = idx["regions"]
    template = idx["tile_url"].split("?")[0]

    keys = _tile_keys_for_bbox(regions_list, south, north, west, east, tile_size)
    if not keys:
        print("No region tiles intersect bbox (check index).", file=sys.stderr)
        sys.exit(1)

    seen: dict[int, dict[str, Any]] = {}
    for key in keys:
        url = template.replace("{lat}/{lon}", key)
        try:
            rows = _get_json(url)
        except Exception as ex:
            print(f"warn: failed {url}: {ex}", file=sys.stderr)
            continue
        if not isinstance(rows, list):
            continue
        for row in rows:
            if args.flock_only and not _is_flock(row.get("tags") or {}):
                continue
            lat, lon = row.get("lat"), row.get("lon")
            if lat is None or lon is None:
                continue
            if not (south <= lat <= north and west <= lon <= east):
                continue
            iid = row.get("id")
            if iid is None:
                continue
            seen[int(iid)] = row

    feats = []
    for row in seen.values():
        tags = row.get("tags") or {}
        manufacturer = tags.get("manufacturer") or tags.get("operator") or ""
        feats.append(
            {
                "type": "Feature",
                "properties": {
                    "id": row.get("id"),
                    "brand": manufacturer or None,
                    "operator": tags.get("operator"),
                    "source": "deflock_cdn",
                    "source_id": str(row.get("id")),
                },
                "geometry": {"type": "Point", "coordinates": [row["lon"], row["lat"]]},
            }
        )

    out = {"type": "FeatureCollection", "features": feats}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(out), encoding="utf-8")
    print(f"Wrote {len(feats)} points to {args.output}")


if __name__ == "__main__":
    main()
