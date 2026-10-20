/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

/**
 * @file mountain.hpp
 * @brief Ridged mountain massif generator.
 *
 * Ported from a static study of Gaea 2.2.6.0 (Mountain): three seeded ridge
 * layers under a radial envelope, optionally eroded by the multiscale particle
 * erosion (see erosion.hpp) with a per-style preset, and terraced for the
 * Strata style.
 */
#pragma once
#include "hesiod/model/nodes/landscape/field.hpp"

namespace hesiod::landscape
{

enum class MountainStyle : int
{
  BASIC = 0,
  ERODED = 1,
  OLD = 2,
  ALPINE = 3,
  STRATA = 4,
};

enum class MountainBulk : int
{
  LOW = 0,
  MEDIUM = 1,
  HIGH = 2,
};

struct MountainParams
{
  float         scale = 0.5f; // radial envelope radius, fraction of the map size
  float         height = 1.25f;
  MountainStyle style = MountainStyle::ERODED;
  MountainBulk  bulk = MountainBulk::MEDIUM;
  bool          reduce_details = false;
  int           seed = 0;
  float         x = 0.5f; // envelope center, unit coordinates
  float         y = 0.5f;
  float         world_width = 5000.f; // meters, used by the erosion presets
  float         world_height = 2500.f;
};

/**
 * @brief Generate the ridged mountain.
 * @param n Field size (power of two, >= 16).
 * @return Heights in absolute units (clamped to [0, 1]).
 */
Field mountain(int n, const MountainParams &p);

} // namespace hesiod::landscape
