#pragma once

#include <cstdint>

// Resolves two-letter state from GPS using `/index/states.bin` (buffered bboxes + centroids).
// When multiple buffered boxes overlap at borders, picks the nearest state centroid.
// Falls back to coarse built-in boxes if states.bin is missing (subset of US only).
bool state_locator_init();
bool state_locator_resolve(double lat_deg, double lon_deg, char out_code[3]);
