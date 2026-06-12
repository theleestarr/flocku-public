#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusFt = 20902231.0;  // mean Earth radius in feet

double deg2rad(double d);
double rad2deg(double r);

// Haversine distance in feet (adequate for sub-mile ALPR lookahead).
double distanceFeet(double lat1_deg, double lon1_deg, double lat2_deg, double lon2_deg);

// Initial bearing from point 1 to point 2, degrees [0, 360).
double bearingDeg(double lat1_deg, double lon1_deg, double lat2_deg, double lon2_deg);

// Smallest absolute difference between two headings [0, 180].
float angleDiffDeg(float a_deg, float b_deg);

// Signed difference bearing - heading, in (-180, 180] (positive = object to the right).
float signedAngleDiffDeg(float heading_deg, float bearing_deg);

// Circular mean of angles in degrees [0, 360), equal weights. count <= 0 returns NaN.
float circularMeanDeg(const float* angles_deg, size_t count);
