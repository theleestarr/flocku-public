"""Geographic bounding-box clips for building regional subsets (same state file, fewer cameras)."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class LonLatBBox:
    """WGS84 axis-aligned clip in degrees: min_lon, min_lat, max_lon, max_lat."""

    min_lon: float
    min_lat: float
    max_lon: float
    max_lat: float

    def contains(self, lat: float, lon: float) -> bool:
        return (
            self.min_lat <= lat <= self.max_lat and self.min_lon <= lon <= self.max_lon
        )


# West edge ~ Cascade crest / Hwy 2 corridor; east to Idaho; state lat bounds.
# Adjust --clip-bbox if you need a tighter county or metro cut.
EASTERN_WA_BBOX = LonLatBBox(min_lon=-121.0, min_lat=45.5, max_lon=-116.9, max_lat=49.0)
