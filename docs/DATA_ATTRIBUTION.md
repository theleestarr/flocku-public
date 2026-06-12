# Camera data attribution

Prebuilt camera packs under `firmware/data/cams/` are derived from community ALPR mapping data fetched via DeFlock’s public CDN (`fetch_deflock_geojson.py`).

- Respect **ODbL** when redistributing or modifying OSM-derived inputs.
- Rebuild with `tools/build_dataset.py` after refreshing source GeoJSON (see `tools/README.md`).
