# Tools

Python 3 — builds **`firmware/data/`** for LittleFS (`pio run -t uploadfs` from `firmware/`).

```bash
pip install -r tools/requirements.txt
```

## Refresh nationwide camera packs

```bash
python3 tools/fetch_deflock_geojson.py --us --flock-only -o tools/data/us_flock_cameras.geojson
python3 tools/build_dataset.py \
  --input tools/data/us_flock_cameras.geojson \
  --states tools/data/ne_110m_admin_1_states_provinces.geojson \
  --output firmware/data
```

This writes all **51** jurisdiction files (`AK` … `WY` + `DC`), **`index/states.bin`** (buffered bboxes + centroids), and bumps `dataset_version` in `index/dataset_manifest.json`.

| Script | Role |
|--------|------|
| `build_dataset.py` | GeoJSON + state polygons → `firmware/data/cams/{ST}.bin`, `index/states.bin`, manifest |
| `fetch_deflock_geojson.py` | DeFlock CDN → GeoJSON for `--input` |
| `image_to_boot_bitmap.py` | PNG → `firmware/src/boot_logo_tracker_bitmap.h` |

**Attribution:** respect **ODbL** for OSM-derived inputs — see [`../docs/DATA_ATTRIBUTION.md`](../docs/DATA_ATTRIBUTION.md).

## Contiguous 48 only

To drop AK/HI from the device (saves a few KB):

```bash
python3 tools/build_dataset.py \
  --input tools/data/us_flock_cameras.geojson \
  --states tools/data/ne_110m_admin_1_states_provinces.geojson \
  --write-states contiguous48 \
  --output firmware/data
```

The default build (no `--write-states`) ships all jurisdictions including sparse AK (empty stub) and HI.
