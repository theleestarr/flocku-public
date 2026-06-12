#include "state_locator.h"

#include <LittleFS.h>

#include <cmath>
#include <cstring>

#include "geo_math.h"

namespace {

#pragma pack(push, 1)
struct BBoxRecord {
  char code[2];
  int32_t min_lat_e7;
  int32_t max_lat_e7;
  int32_t min_lon_e7;
  int32_t max_lon_e7;
  int32_t cent_lat_e7;
  int32_t cent_lon_e7;
};
#pragma pack(pop)

static_assert(sizeof(BBoxRecord) == 26, "BBoxRecord packed size");

constexpr size_t kV1RecordBytes = 18;

// Fallback coarse boxes — only when /index/states.bin is missing (subset of US).
static const BBoxRecord kFallback[] = {
    {{'W', 'A'}, 455000000, 492000000, -1249000000, -1168000000, 474000000, -1205000000},
    {{'O', 'R'}, 415000000, 465000000, -1247000000, -1163000000, 440000000, -1205000000},
    {{'C', 'A'}, 325000000, 422000000, -1245000000, -1140000000, 373500000, -1195000000},
    {{'A', 'K'}, 512000000, 715000000, -1795000000, -1295000000, 640000000, -1530000000},
    {{'H', 'I'}, 187000000, 224000000, -1610000000, -1545000000, 205000000, -1575000000},
    {{'I', 'D'}, 415000000, 495000000, -1173000000, -1110000000, 442000000, -1147000000},
};

static BBoxRecord* g_boxes = nullptr;
static size_t g_nboxes = 0;
static bool g_heap = false;

static bool point_in(const BBoxRecord& b, int32_t lat_e7, int32_t lon_e7) {
  return lat_e7 >= b.min_lat_e7 && lat_e7 <= b.max_lat_e7 && lon_e7 >= b.min_lon_e7 &&
         lon_e7 <= b.max_lon_e7;
}

static void bbox_center_e7(const BBoxRecord& b, int32_t* out_lat, int32_t* out_lon) {
  *out_lat = (b.min_lat_e7 + b.max_lat_e7) / 2;
  *out_lon = (b.min_lon_e7 + b.max_lon_e7) / 2;
}

static uint16_t overlap_priority(char c0, char c1) {
  if (c0 == 'W' && c1 == 'A') return 10;
  if (c0 == 'O' && c1 == 'R') return 15;
  if (c0 == 'C' && c1 == 'A') return 20;
  if (c0 == 'A' && c1 == 'K') return 22;
  if (c0 == 'H' && c1 == 'I') return 24;
  if (c0 == 'M' && c1 == 'T') return 25;
  if (c0 == 'W' && c1 == 'Y') return 28;
  if (c0 == 'N' && c1 == 'V') return 35;
  if (c0 == 'U' && c1 == 'T') return 40;
  if (c0 == 'I' && c1 == 'D') return 45;
  if (c0 == 'C' && c1 == 'O') return 48;
  if (c0 == 'A' && c1 == 'Z') return 52;
  if (c0 == 'N' && c1 == 'M') return 54;
  return 500;
}

static bool better_match(uint16_t new_pri, char na, char nb, uint16_t best_pri, char ba, char bb) {
  if (new_pri < best_pri) return true;
  if (new_pri > best_pri) return false;
  if (na != ba) return na < ba;
  return nb < bb;
}

static void use_fallback() {
  g_boxes = const_cast<BBoxRecord*>(kFallback);
  g_nboxes = sizeof(kFallback) / sizeof(kFallback[0]);
  g_heap = false;
}

static bool load_states_file(File& f) {
  const size_t file_size = f.size();
  if (file_size < sizeof(uint32_t)) return false;

  uint32_t count = 0;
  if (f.read(reinterpret_cast<uint8_t*>(&count), sizeof(count)) != sizeof(count)) return false;
  if (count == 0 || count > 64) return false;

  const size_t payload = file_size - sizeof(uint32_t);
  const size_t rec_size = payload / count;
  if (rec_size != kV1RecordBytes && rec_size != sizeof(BBoxRecord)) return false;
  if (payload != count * rec_size) return false;

  g_boxes = new BBoxRecord[count];
  g_nboxes = count;
  g_heap = true;

  for (uint32_t i = 0; i < count; ++i) {
    BBoxRecord rec{};
    if (rec_size == sizeof(BBoxRecord)) {
      if (f.read(reinterpret_cast<uint8_t*>(&rec), sizeof(BBoxRecord)) != sizeof(BBoxRecord)) {
        delete[] g_boxes;
        g_boxes = nullptr;
        g_nboxes = 0;
        g_heap = false;
        return false;
      }
    } else {
      char code[2];
      int32_t mnlat, mxlat, mnlon, mxlon;
      if (f.read(reinterpret_cast<uint8_t*>(code), 2) != 2 ||
          f.read(reinterpret_cast<uint8_t*>(&mnlat), sizeof(mnlat)) != sizeof(mnlat) ||
          f.read(reinterpret_cast<uint8_t*>(&mxlat), sizeof(mxlat)) != sizeof(mxlat) ||
          f.read(reinterpret_cast<uint8_t*>(&mnlon), sizeof(mnlon)) != sizeof(mnlon) ||
          f.read(reinterpret_cast<uint8_t*>(&mxlon), sizeof(mxlon)) != sizeof(mxlon)) {
        delete[] g_boxes;
        g_boxes = nullptr;
        g_nboxes = 0;
        g_heap = false;
        return false;
      }
      rec.code[0] = code[0];
      rec.code[1] = code[1];
      rec.min_lat_e7 = mnlat;
      rec.max_lat_e7 = mxlat;
      rec.min_lon_e7 = mnlon;
      rec.max_lon_e7 = mxlon;
      bbox_center_e7(rec, &rec.cent_lat_e7, &rec.cent_lon_e7);
    }
    g_boxes[i] = rec;
  }
  return true;
}

}  // namespace

bool state_locator_init() {
  if (g_boxes && g_heap) {
    delete[] g_boxes;
    g_boxes = nullptr;
    g_nboxes = 0;
    g_heap = false;
  }

  if (!LittleFS.exists("/index/states.bin")) {
    use_fallback();
    return true;
  }

  File f = LittleFS.open("/index/states.bin", "r", false);
  if (!f || !load_states_file(f)) {
    if (f) f.close();
    use_fallback();
    return true;
  }
  f.close();
  return true;
}

bool state_locator_resolve(double lat_deg, double lon_deg, char out_code[3]) {
  out_code[0] = '-';
  out_code[1] = '-';
  out_code[2] = '\0';

  const int32_t lat_e7 = static_cast<int32_t>(lat_deg * 1e7);
  const int32_t lon_e7 = static_cast<int32_t>(lon_deg * 1e7);

  if (!g_boxes) use_fallback();

  size_t match_idx[64];
  size_t nmatch = 0;
  for (size_t i = 0; i < g_nboxes; ++i) {
    if (!point_in(g_boxes[i], lat_e7, lon_e7)) continue;
    if (nmatch < sizeof(match_idx) / sizeof(match_idx[0])) match_idx[nmatch++] = i;
  }

  if (nmatch == 0) return false;

  size_t best_i = match_idx[0];
  if (nmatch == 1) {
    out_code[0] = g_boxes[best_i].code[0];
    out_code[1] = g_boxes[best_i].code[1];
    out_code[2] = '\0';
    return true;
  }

  double best_dist = 1e18;
  uint16_t best_pri = 65535;
  char best_a = 'Z';
  char best_b = 'Z';

  for (size_t mi = 0; mi < nmatch; ++mi) {
    const BBoxRecord& b = g_boxes[match_idx[mi]];
    const double clat = static_cast<double>(b.cent_lat_e7) / 1e7;
    const double clon = static_cast<double>(b.cent_lon_e7) / 1e7;
    const double dist = distanceFeet(lat_deg, lon_deg, clat, clon);
    const char ca = b.code[0];
    const char cb = b.code[1];
    const uint16_t pri = overlap_priority(ca, cb);

    if (dist < best_dist - 1.0 ||
        (std::abs(dist - best_dist) <= 1.0 && better_match(pri, ca, cb, best_pri, best_a, best_b))) {
      best_i = match_idx[mi];
      best_dist = dist;
      best_pri = pri;
      best_a = ca;
      best_b = cb;
    }
  }

  out_code[0] = g_boxes[best_i].code[0];
  out_code[1] = g_boxes[best_i].code[1];
  out_code[2] = '\0';
  return true;
}
