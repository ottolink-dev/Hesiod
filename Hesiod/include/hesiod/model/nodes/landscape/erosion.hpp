/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

/**
 * @file erosion.hpp
 * @brief Multiscale particle erosion.
 *
 * Ported from a static study of Gaea 2.2.6.0 (Erosion2 / ParallelErosion CPU
 * path): the terrain is decomposed into a downsampled pyramid and eroded from
 * coarse to fine by particles that cut toward the height a few steps
 * downstream and drop sediment by slope thresholds (suspended load, bed load,
 * coarse sediments), with a shaping pass after every particle pass. A spawn
 * mask ("precipitation") restricts where particles are released.
 */
#pragma once
#include "hesiod/model/nodes/landscape/field.hpp"

namespace hesiod::landscape
{

struct ErosionParams
{
  float duration = 10.f;
  float downcutting = 0.25f;
  float erosion_scale = 1000.f; // meters
  float suspended_amount = 1.f;
  float suspended_angle = 24.f; // degrees
  float bed_load_amount = 0.f;
  float bed_load_angle = 15.f;
  float coarse_amount = 0.f;
  float coarse_angle = 10.f;
  float shape = 0.2f;
  float shape_sharpness = 0.6f;
  float shape_detail_scale = 0.25f;
  int   seed = 0;
  float world_width = 5000.f;
  float world_height = 2500.f;
};

struct ErosionResult
{
  Field height;
  Field flow;     // particle visit count at the finest level
  Field deposits; // accumulated sediment
  Field wear;     // accumulated material removal
};

struct PrecipitationParams
{
  float slope_min = 0.f; // degrees
  float slope_max = 90.f;
  float altitude_min = 0.f; // normalized height
  float altitude_max = 1.f;
  bool  directional = false;
  float direction = 0.f; // degrees, direction the moist air comes from
  float rain_shadow = 0.8f;
  bool  reverse = false;
  float world_width = 5000.f;
  float world_height = 2500.f;
};

/**
 * @brief Multiscale particle erosion.
 * @param height Input heights, n x n with n a power of two.
 * @param p_mask Optional spawn-acceptance mask in [0, 1] (same size as @p height):
 *               a particle is only released where a random draw falls below it.
 */
ErosionResult erode(Field height, const ErosionParams &p, const Field *p_mask = nullptr);

/**
 * @brief Build the precipitation mask from slope / altitude windows and an
 * optional directional rain shadow.
 */
Field precipitation_mask(const Field &height, const PrecipitationParams &p);

} // namespace hesiod::landscape
