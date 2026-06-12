#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

_TOOLS_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _TOOLS_DIR.parent
if str(_TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(_TOOLS_DIR))

from alpr_dataset.cluster import CamPoint, cluster_cameras
from alpr_dataset.export_binary import write_camera_bin, write_states_bin
from alpr_dataset.geo_clip import EASTERN_WA_BBOX, LonLatBBox
from alpr_dataset.manifest import next_dataset_version, write_dataset_manifest


def _build_states_bbox_index(states_path: Path, buffer_miles: float, out_dir: Path) -> dict:
    """Write /index/states.bin only: buffered WGS84 bbox per state row in --states (no camera input)."""
    from alpr_dataset.state_buffer import load_states, state_index_record_e7

    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "index").mkdir(parents=True, exist_ok=True)

    states = load_states(states_path, buffer_miles=buffer_miles)
    boxes = [state_index_record_e7(s) for s in states]
    idx_bytes = write_states_bin(out_dir / "index" / "states.bin", boxes)

    detail: dict[str, dict[str, float]] = {}
    for box in boxes:
        code, mnlat, mxlat, mnlon, mxlon = box
        detail[code] = {
            "min_lat": mnlat / 1e7,
            "max_lat": mxlat / 1e7,
            "min_lon": mnlon / 1e7,
            "max_lon": mxlon / 1e7,
        }

    report = {
        "mode": "bbox_index_only",
        "buffer_miles": buffer_miles,
        "state_count": len(states),
        "bboxes_deg": detail,
        "index_bytes": idx_bytes,
        "total_output_bytes": idx_bytes,
    }
    (out_dir / "build_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def _demo_dataset(out_dir: Path, region: str = "seattle", *, dataset_version: int = 1) -> dict:
    """Tiny synthetic WA scene: 4 cameras in a tight pattern to exercise clustering."""
    out_dir.mkdir(parents=True, exist_ok=True)
    if region == "eastern-wa":
        base_lat, base_lon = 47.6588, -117.4260  # Spokane — inside EASTERN_WA_BBOX
    else:
        base_lat, base_lon = 47.6200, -122.3500  # Seattle (west)
    pts = [
        CamPoint(base_lat + 0.00010, base_lon + 0.00005, 1),
        CamPoint(base_lat - 0.00005, base_lon + 0.00012, 1),
        CamPoint(base_lat + 0.00002, base_lon - 0.00010, 2),
        CamPoint(base_lat - 0.00012, base_lon - 0.00002, 2),
    ]
    alerts = cluster_cameras(pts, cluster_radius_ft=200.0)

    cams_dir = out_dir / "cams"
    index_dir = out_dir / "index"
    wa_bytes = write_camera_bin(cams_dir / "WA.bin", "WA", alerts, dataset_version=dataset_version)

    # Minimal /index/states.bin for firmware (one WA bbox in e7).
    boxes = [
        ("WA", int(round(45.5 * 1e7)), int(round(49.2 * 1e7)), int(round(-124.9 * 1e7)), int(round(-116.8 * 1e7)),
         int(round(47.4 * 1e7)), int(round(-120.5 * 1e7))),
    ]
    idx_bytes = write_states_bin(index_dir / "states.bin", boxes)

    per_state = {
        "WA": {
            "raw": len(pts),
            "buffered": len(pts),
            "alert_objects": len(alerts),
            "bytes": wa_bytes,
        }
    }
    write_dataset_manifest(out_dir, dataset_version, per_state, source="demo")
    report = {
        "dataset_version": dataset_version,
        "total_raw_cameras": len(pts),
        "total_alert_objects": len(alerts),
        "demo_region": region,
        "states": per_state,
        "index_bytes": idx_bytes,
        "total_output_bytes": wa_bytes + idx_bytes,
    }
    (out_dir / "build_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def _real_build(
    input_path: Path,
    states_path: Path,
    buffer_miles: float,
    cluster_radius_ft: float,
    out_dir: Path,
    clip: LonLatBBox | None,
    write_states_filter: frozenset[str] | None,
    *,
    dataset_version: int,
) -> dict:
    from alpr_dataset.load_cameras import iter_geojson_features
    from alpr_dataset.normalize import RawCamera, classify_camera_type, normalize_feature
    from alpr_dataset.state_buffer import assign_cameras_to_states, load_states, state_index_record_e7

    out_dir.mkdir(parents=True, exist_ok=True)

    raw_cams_all: list[RawCamera] = []
    for feat in iter_geojson_features(input_path):
        cam = normalize_feature(feat)
        if cam:
            raw_cams_all.append(cam)

    dropped_clip = 0
    raw_cams = raw_cams_all
    if clip is not None:
        raw_cams = []
        for c in raw_cams_all:
            if clip.contains(c.lat, c.lon):
                raw_cams.append(c)
            else:
                dropped_clip += 1

    states = load_states(states_path, buffer_miles=buffer_miles)
    triples: list[tuple[float, float, RawCamera]] = [(c.lat, c.lon, c) for c in raw_cams]
    by_state = assign_cameras_to_states(triples, states)

    per_state_out: dict[str, dict] = {}
    total_alerts = 0
    total_bytes = 0

    cams_dir = out_dir / "cams"
    cams_dir.mkdir(parents=True, exist_ok=True)
    if write_states_filter is not None:
        for p in cams_dir.glob("*.bin"):
            p.unlink()

    for st in states:
        if write_states_filter is not None and st.code not in write_states_filter:
            continue
        cams = by_state.get(st.code, [])
        pts = [CamPoint(c.lat, c.lon, classify_camera_type(c)) for c in cams]
        alerts = cluster_cameras(pts, cluster_radius_ft=cluster_radius_ft)
        p = cams_dir / f"{st.code}.bin"
        b = write_camera_bin(p, st.code, alerts, dataset_version=dataset_version)
        per_state_out[st.code] = {
            "raw": len(cams),
            "buffered": len(cams),
            "alert_objects": len(alerts),
            "bytes": b,
        }
        total_alerts += len(alerts)
        total_bytes += b

    states_for_index = states
    if write_states_filter is not None:
        states_for_index = [s for s in states if s.code in write_states_filter]
    boxes = [state_index_record_e7(s) for s in states_for_index]
    idx_bytes = write_states_bin(out_dir / "index" / "states.bin", boxes)
    total_bytes += idx_bytes

    write_dataset_manifest(
        out_dir,
        dataset_version,
        per_state_out,
        source=str(input_path),
        extra={"total_raw_cameras": len(raw_cams), "total_alert_objects": total_alerts},
    )

    report = {
        "dataset_version": dataset_version,
        "total_raw_cameras": len(raw_cams),
        "total_raw_before_clip": len(raw_cams_all),
        "dropped_by_clip": dropped_clip,
        "clip": (
            {
                "min_lon": clip.min_lon,
                "min_lat": clip.min_lat,
                "max_lon": clip.max_lon,
                "max_lat": clip.max_lat,
            }
            if clip
            else None
        ),
        "total_alert_objects": total_alerts,
        "states": per_state_out,
        "index_bytes": idx_bytes,
        "total_output_bytes": total_bytes,
    }
    (out_dir / "build_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def _print_summary(report: dict) -> None:
    if report.get("mode") == "bbox_index_only":
        print(f"bbox index: {report['state_count']} states, {report['index_bytes']} bytes -> index/states.bin")
        print(f"buffer_miles={report['buffer_miles']}")
        return
    if "total_raw_before_clip" in report:
        print(f"Raw cameras (before clip): {report['total_raw_before_clip']}")
        print(f"Dropped by clip: {report['dropped_by_clip']}")
    print(f"Total raw cameras: {report['total_raw_cameras']}")
    if report.get("clip"):
        c = report["clip"]
        print(
            f"Clip: lon [{c['min_lon']}, {c['max_lon']}] lat [{c['min_lat']}, {c['max_lat']}]"
        )
    if "dataset_version" in report:
        print(f"Dataset version: {report['dataset_version']}")
    print(f"Total alert objects: {report['total_alert_objects']}")
    for code, row in sorted(report.get("states", {}).items()):
        print(
            f"{code} raw: {row.get('raw')} buffered: {row.get('buffered')} "
            f"alerts: {row.get('alert_objects')} bytes: {row.get('bytes')}"
        )
    print(f"index/states.bin bytes: {report.get('index_bytes')}")
    print(f"Total output size: {report.get('total_output_bytes')} bytes")


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Build firmware-compatible ALPR camera LittleFS dataset.",
        epilog="Eastern WA: use --eastern-wa to keep only cameras in roughly lon>=-121 (WA). "
        "Firmware still loads /cams/WA.bin for the whole state code. "
        "All US states: use nationwide camera GeoJSON (e.g. fetch_deflock_geojson.py --us), "
        "omit --eastern-wa/--clip-bbox, and leave --write-states empty to emit every ST.bin present in --states.",
    )
    ap.add_argument("--demo", action="store_true", help="Write a tiny synthetic WA dataset (no GeoJSON needed).")
    ap.add_argument(
        "--bbox-index-only",
        action="store_true",
        help="Only write index/states.bin from --states polygons (buffered bboxes). No --input.",
    )
    ap.add_argument(
        "--demo-region",
        choices=("seattle", "eastern-wa"),
        default="seattle",
        help="With --demo: where to place synthetic points (default: seattle).",
    )
    ap.add_argument("--input", type=Path, help="Camera GeoJSON or .geojson.gz")
    ap.add_argument("--states", type=Path, help="US states polygons (GeoPackage/GeoJSON shapefile via geopandas)")
    ap.add_argument("--buffer-miles", type=float, default=25.0)
    ap.add_argument("--cluster-radius-ft", type=float, default=200.0)
    ap.add_argument("--output", type=Path, default=_REPO_ROOT / "firmware" / "data")
    ap.add_argument(
        "--eastern-wa",
        action="store_true",
        help="Clip cameras to Eastern Washington (WGS84 ~ lon [-121,-116.9], lat [45.5,49]).",
    )
    ap.add_argument(
        "--clip-bbox",
        nargs=4,
        type=float,
        metavar=("MIN_LON", "MIN_LAT", "MAX_LON", "MAX_LAT"),
        help="Custom clip (degrees). Incompatible with --eastern-wa.",
    )
    ap.add_argument(
        "--write-states",
        type=str,
        default="",
        help="Comma-separated state codes, or alias: west-coast (= WA,OR,CA,AK,HI), "
        "northwest5 (= WA,ID,MT,CA,OR), contiguous48 (= all US states except AK,HI; includes DC). "
        "Empty = all states from --states polygons.",
    )
    ap.add_argument(
        "--dataset-version",
        type=int,
        default=None,
        help="Monotonic camera DB version stamped into each ST.bin and index/dataset_manifest.json. "
        "Default: previous manifest version + 1 (or 1 on first build).",
    )
    args = ap.parse_args()

    manifest_path = args.output / "index" / "dataset_manifest.json"
    dataset_version = next_dataset_version(manifest_path, args.dataset_version)

    if args.demo:
        report = _demo_dataset(args.output, region=args.demo_region, dataset_version=dataset_version)
        _print_summary(report)
        return

    if args.bbox_index_only:
        if not args.states:
            ap.error("--bbox-index-only requires --states")
        if args.input:
            ap.error("--bbox-index-only cannot be used with --input")
        if args.eastern_wa or args.clip_bbox:
            ap.error("--bbox-index-only does not use camera clip flags")
        if args.write_states.strip():
            ap.error("--bbox-index-only does not use --write-states")
        report = _build_states_bbox_index(args.states, args.buffer_miles, args.output)
        _print_summary(report)
        return

    if not args.input or not args.states:
        ap.error("--input and --states are required unless --demo or --bbox-index-only is set")

    if args.eastern_wa and args.clip_bbox:
        ap.error("use only one of --eastern-wa and --clip-bbox")

    clip: LonLatBBox | None
    if args.eastern_wa:
        clip = EASTERN_WA_BBOX
    elif args.clip_bbox:
        lo_mi, la_mi, lo_ma, la_ma = args.clip_bbox
        clip = LonLatBBox(min_lon=lo_mi, min_lat=la_mi, max_lon=lo_ma, max_lat=la_ma)
    else:
        clip = None

    write_states_filter: frozenset[str] | None = None
    ws_arg = args.write_states.strip()
    if ws_arg:
        key = ws_arg.lower().replace("_", "-")
        if key in ("west-coast", "pacific"):
            write_states_filter = frozenset({"WA", "OR", "CA", "AK", "HI"})
        elif key in ("northwest5", "pnw5", "wa-id-mt-ca-or"):
            write_states_filter = frozenset({"WA", "ID", "MT", "CA", "OR"})
        elif key in ("contiguous48", "conus48", "lower48"):
            write_states_filter = frozenset(
                {
                    "AL", "AZ", "AR", "CA", "CO", "CT", "DC", "DE", "FL", "GA",
                    "IA", "ID", "IL", "IN", "KS", "KY", "LA", "MA", "MD", "ME",
                    "MI", "MN", "MO", "MS", "MT", "NC", "ND", "NE", "NH", "NJ",
                    "NM", "NV", "NY", "OH", "OK", "OR", "PA", "RI", "SC", "SD",
                    "TN", "TX", "UT", "VA", "VT", "WA", "WI", "WV", "WY",
                }
            )
        else:
            write_states_filter = frozenset(x.strip().upper() for x in ws_arg.split(",") if x.strip())

    report = _real_build(
        args.input,
        args.states,
        buffer_miles=args.buffer_miles,
        cluster_radius_ft=args.cluster_radius_ft,
        out_dir=args.output,
        clip=clip,
        write_states_filter=write_states_filter,
        dataset_version=dataset_version,
    )
    _print_summary(report)


if __name__ == "__main__":
    main()
