#include "camera_store.h"

#include <LittleFS.h>

#include <cmath>
#include <cstring>

#include "geo_math.h"

bool CameraStore::mountLittleFs() {
  // Partition **name** in partitions.csv is `littlefs` (subtype keyword "spiffs" is legacy only).
  // Default LittleFS.begin() uses label "spiffs", which does not exist → mount always failed.
  mounted_ = LittleFS.begin(false, "/littlefs", 10, "littlefs");
  return mounted_;
}

bool CameraStore::read_header() {
  header_ok_ = false;
  if (!f_) return false;
  if (f_.read(reinterpret_cast<uint8_t*>(&hdr_), sizeof(hdr_)) != sizeof(hdr_)) return false;
  if (std::strncmp(hdr_.magic, kCameraMagic, 4) != 0) return false;
  if (hdr_.version != kCameraFileVersion) return false;
  header_ok_ = true;
  return true;
}

bool CameraStore::seek_to_records() { return f_.seek(sizeof(CameraFileHeader)); }

void CameraStore::close() {
  if (f_) f_.close();
  header_ok_ = false;
}

bool CameraStore::open_state_file(const char state_two_letter[2]) {
  close();
  if (!mounted_) return false;
  char path[32];
  snprintf(path, sizeof(path), "/cams/%c%c.bin", state_two_letter[0], state_two_letter[1]);
  f_ = LittleFS.open(path, "r", false);
  if (!f_) return false;
  if (!read_header()) return false;
  if (!seek_to_records()) return false;
  return true;
}

void CameraStore::scan_near(double lat_deg, double lon_deg, float max_ft, ScanCallback cb, void* user) {
  if (!header_ok_ || !cb || !f_) return;

  // ~1 deg lat ~ 365228 ft → coarse margin from max_ft
  const double lat_margin = (static_cast<double>(max_ft) + 500.0) / 365228.0;
  const double cos_lat = std::cos(lat_deg * (3.14159265358979323846 / 180.0));
  const double lon_scale = std::max(0.2, cos_lat);
  const double lon_margin = lat_margin / lon_scale;

  const double min_lat = lat_deg - lat_margin;
  const double max_lat = lat_deg + lat_margin;
  const double min_lon = lon_deg - lon_margin;
  const double max_lon = lon_deg + lon_margin;

  if (!f_.seek(sizeof(CameraFileHeader))) return;

  for (uint32_t i = 0; i < hdr_.record_count; ++i) {
    AlertObject obj{};
    if (f_.read((uint8_t*)&obj, sizeof(obj)) != sizeof(obj)) break;
    const double olat = static_cast<double>(obj.lat_e7) / 1e7;
    const double olon = static_cast<double>(obj.lon_e7) / 1e7;
    if (olat < min_lat || olat > max_lat || olon < min_lon || olon > max_lon) continue;
    const double d = distanceFeet(lat_deg, lon_deg, olat, olon);
    if (d > static_cast<double>(max_ft) + static_cast<double>(obj.radius_ft)) continue;
    cb(obj, user);
  }
}
