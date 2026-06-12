#include "geo_math.h"

double deg2rad(double d) { return d * (kPi / 180.0); }
double rad2deg(double r) { return r * (180.0 / kPi); }

double distanceFeet(double lat1_deg, double lon1_deg, double lat2_deg, double lon2_deg) {
  const double dlat = deg2rad(lat2_deg - lat1_deg);
  const double dlon = deg2rad(lon2_deg - lon1_deg);
  const double a =
      std::sin(dlat / 2) * std::sin(dlat / 2) +
      std::cos(deg2rad(lat1_deg)) * std::cos(deg2rad(lat2_deg)) * std::sin(dlon / 2) * std::sin(dlon / 2);
  const double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  return kEarthRadiusFt * c;
}

double bearingDeg(double lat1_deg, double lon1_deg, double lat2_deg, double lon2_deg) {
  const double phi1 = deg2rad(lat1_deg);
  const double phi2 = deg2rad(lat2_deg);
  const double dlon = deg2rad(lon2_deg - lon1_deg);
  const double y = std::sin(dlon) * std::cos(phi2);
  const double x = std::cos(phi1) * std::sin(phi2) - std::sin(phi1) * std::cos(phi2) * std::cos(dlon);
  double brng = std::atan2(y, x);
  brng = rad2deg(brng);
  brng = std::fmod(brng + 360.0, 360.0);
  return brng;
}

float angleDiffDeg(float a_deg, float b_deg) {
  float d = std::fabs(a_deg - b_deg);
  return d > 180.0f ? (360.0f - d) : d;
}

float signedAngleDiffDeg(float heading_deg, float bearing_deg) {
  float d = bearing_deg - heading_deg;
  while (d > 180.0f) d -= 360.0f;
  while (d <= -180.0f) d += 360.0f;
  return d;
}

float circularMeanDeg(const float* angles_deg, size_t count) {
  if (!angles_deg || count == 0) return NAN;
  double sx = 0.0, sy = 0.0;
  for (size_t i = 0; i < count; ++i) {
    const double rad = deg2rad(static_cast<double>(angles_deg[i]));
    sx += std::cos(rad);
    sy += std::sin(rad);
  }
  double mean = std::atan2(sy, sx);
  mean = rad2deg(mean);
  mean = std::fmod(mean + 360.0, 360.0);
  if (mean < 0) mean += 360.0;
  return static_cast<float>(mean);
}
