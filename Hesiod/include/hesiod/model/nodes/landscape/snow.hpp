/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

/**
 * @file snow.hpp
 * @brief Snowfall, avalanche settling and melt on a heightfield.
 *
 * Port of the snow solver studied in Gaea 2.2.6.0 (Snow node, Gaea.CS.dll
 * Erosion1): snow falls above a snow line on the coarsest level of a Gaussian
 * pyramid, slides wherever the snow surface is steeper than a depth-dependent
 * slip-off slope, partially melts, and is pushed back up the pyramid while the
 * finer levels only re-settle it. Heights are handled in meters internally.
 */
#pragma once
#include "hesiod/model/nodes/landscape/field.hpp"

namespace hesiod::landscape
{

struct SnowParams
{
  float duration = 0.25f;         // snowfall duration
  float intensity = 0.25f;        // snowfall intensity
  float settle = 0.25f;           // settle (post-snowfall) duration
  float melt = 0.f;               // melt amount
  float snow_line = 0.f;          // fraction of the terrain height
  float slip_off_angle = 38.f;    // degrees
  float adhered_snow_mass = 12.f; // snow depth (m) that still holds on slopes
  float model_scale = 5.f;
  float terrain_width = 5000.f; // meters
  float terrain_height = 2500.f;
};

struct SnowResult
{
  Field height; // terrain + visible snow, input units
  Field snow;   // soft snow coverage in [0, 1]
  Field hard;   // binary snow presence
  Field depth;  // visible snow depth, input units
};

/**
 * @param height Terrain in input units (1.0 = terrain_height meters), n x n.
 * @param p_snowfall Optional snowfall multiplier in [0, 1].
 * @param p_melt Optional melt multiplier in [0, 1].
 */
SnowResult snow(const Field      &height,
                const SnowParams &p,
                const Field      *p_snowfall = nullptr,
                const Field      *p_melt = nullptr);

struct SunExposureParams
{
  int   day_begin = 36; // day of the year
  int   day_end = 146;
  float latitude = 65.f; // degrees
  float north = 0.f;     // degrees, rotates the sun path
  float terrain_width = 5000.f;
  float terrain_height = 2500.f;
};

/**
 * @brief Relative solar energy received over a range of days (sun path
 * sampling with cast shadows), normalized to [0, 1].
 */
Field sun_exposure(const Field &height, const SunExposureParams &p);

/**
 * @brief Gaea's directional melt map: histogram-equalized sun exposure, lightly
 * blurred, plus a constant remnant, clamped to [0, 1].
 */
Field directional_melt_map(const Field &height,
                           float        direction,
                           float        remnants,
                           float        terrain_width,
                           float        terrain_height);

} // namespace hesiod::landscape
