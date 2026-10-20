/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <array>
#include <cmath>

#include "hesiod/model/nodes/landscape/erosion.hpp"
#include "hesiod/model/nodes/landscape/mountain.hpp"

#include "noise.hpp"

namespace hesiod::landscape
{

using namespace detail;

namespace
{

// --- profiles ---

// Gaea Profiles.Terrace: randomized level set, smoothstep between levels
Field terrace(Field g,
              int   count,
              float uniformity,
              float steepness,
              float intensity,
              int   seed)
{
  std::vector<float> levels(count);
  float              step = 1.f / float(count - 1);
  for (int i = 1; i < count; i++)
    levels[i] = levels[i - 1] + step;

  Rng rng(seed);
  for (int pass = 0; pass < 10; pass++)
    for (int i = 1; i < count - 1; i++)
    {
      float h = levels[i - 1] + (levels[i + 1] - levels[i - 1]) * rng.next();
      levels[i] = levels[i] * uniformity + h * (1.f - uniformity);
    }

  for (float &v : g.data)
  {
    if (v >= .999f)
      continue;

    int i = 0;
    for (int k = 0; k < count - 1; k++)
    {
      if (levels[i + 1] > v || i + 1 == count)
        break;
      i++;
    }
    i = std::min(i, count - 2);

    float t = (v - levels[i]) / (levels[i + 1] - levels[i]);
    float s = 3.f * t * t - 2.f * t * t * t;
    s = (s - .5f) * 2.f;
    bool neg = s < 0.f;
    s = std::abs(s);
    if (neg)
      s = -s;
    s = s / 2.f + .5f;
    s = std::pow(s, 1.f - steepness);
    v = (levels[i] * (1.f - s) + levels[i + 1] * s) * intensity + v * (1.f - intensity);
  }
  return g;
}

Field direction_warp(const Field &g,
                     const Field &mod,
                     float        strength,
                     float        angle,
                     int          edge_mode)
{
  Field o(g.n);
  float mx = -std::cos(angle * PI / 180.f), my = std::sin(angle * PI / 180.f);
  parallel_rows(g.n,
                [&](int y)
                {
                  for (int x = 0; x < g.n; x++)
                  {
                    float d = (mod.at(x, y) - .5f) * strength;
                    o.at(x, y) = bilinear(g.data,
                                          g.n,
                                          float(x) + mx * d,
                                          float(y) + my * d,
                                          edge_mode);
                  }
                });
  return o;
}

// Gaea Landscapes.Ridge
Field ridge(int n, float scale, float height, float definition, int seed)
{
  Field v = voronoi(n, 1.f - scale * 1.05f, seed);
  Field p = perlin(n, .75f, seed + 1);
  Field t = terrace(std::move(p), 67, .6f, .2f, .8f, seed + 2);
  t = combine(t,
              fractal_warp(t, 1.f - definition * .99f, 1.f, 0, .5f, seed + 3, 0),
              true);

  Field a = direction_warp(v, t, definition * 1.25f * float(n), 45.f, 1);
  Field b = fractal_warp(a, scale / 2.f, .65f, 2, .4f, seed + 4, 1);
  a = combine(a, b, false);

  for (float &h : a.data)
    h *= scale;
  range_shift_clamp(a);
  for (float &h : a.data)
    h *= height;
  return a;
}

// Engine ShaperCore exponential remap, a = -0.25 (Low) or 0.2 (High)
void bulk_adjustment(Field &g, float a)
{
  if (a == 0.f)
    return;

  auto  mm = std::minmax_element(g.data.begin(), g.data.end());
  float m = *mm.first, r = *mm.second - m;
  if (std::abs(r) < 0.00001f)
    return;

  float k = -10.f * a;
  float denom = float(std::exp(k)) - 1.f;
  for (float &x : g.data)
  {
    float t = (x - m) / r;
    x = m + r * (float(std::exp(k * t)) - 1.f) / denom;
  }
}

// --- Strata: Core.FractalTerraces Process2 / terrace2 and ComplexTerraces ---

struct TerraceContext
{
  std::array<uint8_t, 512> rnd{};
  float                    spacing = .08f;
  float                    intensity = .5f;
  float                    shape = .4f;
  int                      octaves = 12;
};

float fractal_terrace_value(float height, const TerraceContext &c)
{
  struct Point
  {
    float h = 0.f, pos = 0.f;
    int   hash = 0;
  };

  float value = height / c.spacing;
  int   cell = int(value + 1000.f) - 999;
  Point left, right;

  do
  {
    cell--;
    left.hash = (cell & 255) + 11;
    right.hash = ((cell + 1) & 255) + 11;
    left.h = float(cell) + float(c.rnd[left.hash]) / 255.f;
    right.h = float(cell + 1) + float(c.rnd[right.hash]) / 255.f;
    left.pos = left.h;
    right.pos = right.h;
  } while (value < left.pos);

  for (int octave = 0; octave < c.octaves; octave++)
  {
    float t = (value - left.pos) / (right.pos - left.pos);
    float cut = float(c.rnd[left.hash]) / 255.f;
    float shape = std::clamp((-2.f * (float(c.rnd[right.hash]) / 255.f) + 1.f) *
                                     (2.f - std::abs(c.shape)) +
                                 c.shape,
                             -1.f,
                             1.f);
    shape *= std::min(cut, 1.f - cut) * c.intensity;

    if (t < cut)
    {
      right.h = left.h + (right.h - left.h) * (cut + shape);
      right.pos = left.pos + (right.pos - left.pos) * cut;
      right.hash = left.hash;
    }
    else
    {
      left.h += (right.h - left.h) * (cut + shape);
      left.pos += (right.pos - left.pos) * cut;
    }
    left.hash = c.rnd[left.hash + 11];
    right.hash = c.rnd[right.hash + 13];
  }

  float t = right.pos > left.pos ? (value - left.pos) / (right.pos - left.pos) : left.pos;
  return c.spacing * (left.h + (right.h - left.h) * t);
}

Field linear_gradient(int n, int direction)
{
  Field g(n);
  float ax = -std::sin(float(direction) * .01745329238474369f);
  float ay = std::cos(float(direction) * .01745329238474369f);
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
    {
      float v = -((float(x) - float(n) * .5f) * ax + (float(y) - float(n) * .5f) * ay) /
                    float(n) +
                .5f;
      v *= .5f;
      v -= std::floor(v);
      v *= 2.f;
      if (v > 1.f)
        v = 2.f - v;
      g.at(x, y) = v;
    }
  return g;
}

Field fractal_terrace_pass(Field input,
                           float spacing,
                           float intensity,
                           float shape,
                           int   seed,
                           float tilt_amount,
                           int   tilt_seed,
                           int   direction)
{
  Field tilt = linear_gradient(input.n, direction);
  if (tilt_amount < 1.f)
    for (float &v : tilt.data)
      v *= tilt_amount;
  tilt = fractal_warp(tilt, .33f, .33f, 0, .5f, tilt_seed, 1);

  for (size_t i = 0; i < input.data.size(); i++)
    input.data[i] += tilt.data[i];

  TerraceContext c;
  c.spacing = std::max(.001f, spacing);
  c.intensity = intensity;
  c.shape = shape;
  Rng rng(seed);
  for (auto &v : c.rnd)
    v = uint8_t(rng.integer() & 255);

  Field out(input.n);
  parallel_rows(input.n,
                [&](int y)
                {
                  for (int x = 0; x < input.n; x++)
                    out.at(x, y) = fractal_terrace_value(input.at(x, y), c);
                });

  auto  in_range = std::minmax_element(input.data.begin(), input.data.end());
  auto  out_range = std::minmax_element(out.data.begin(), out.data.end());
  float il = *in_range.first, ih = *in_range.second;
  float ol = *out_range.first, oh = *out_range.second;
  float multiplier = (oh == ol) ? 0.f : (ih - il) / (oh - ol);
  float item = il - ol * multiplier;

  for (size_t i = 0; i < out.data.size(); i++)
    out.data[i] = out.data[i] * multiplier + item - tilt.data[i];
  return out;
}

Field complex_terraces(const Field &input, int seed)
{
  Rng   rng(seed);
  int   direction = 0;
  float tilt = .65f + rng.range(.3f, .6f);
  int   ts = rng.integer();
  Field a = fractal_terrace_pass(input, .08f, .5f, .4f, seed, tilt, ts, direction);

  direction += rng.rangei(5, 15);
  float spacing = .08f + .08f * rng.range(.01f, .2f);
  tilt = .65f + rng.range(.3f, .6f);
  ts = rng.integer();
  Field b = fractal_terrace_pass(a, spacing, .5f, .4f, seed, tilt, ts, direction);
  Field merged = combine(a, b, false);

  int direction3 = direction + rng.rangei(25, 45);
  spacing = .08f + .08f * rng.range(.1f, .4f);
  tilt = .65f + rng.range(.3f, .6f);
  ts = rng.integer();
  Field c = fractal_terrace_pass(merged, spacing, .5f, .4f, seed, tilt, ts, direction3);
  return combine(merged, c, false);
}

// blend toward the terraces in the upper normalized height band
Field strata(Field input, int seed)
{
  Field t = complex_terraces(input, seed);
  Field mask = input;
  normalize(mask);

  for (size_t i = 0; i < input.data.size(); i++)
  {
    float z = mask.data[i], w;
    if (z < .65f - .33f || z > 1.f + .33f)
      w = 0.f;
    else if (z < .65f)
      w = (z - (.65f - .33f)) / .33f;
    else if (z > 1.f)
      w = 1.f - (z - 1.f) / .33f;
    else
      w = 1.f;
    input.data[i] = input.data[i] * (1.f - w) + t.data[i] * w;
  }
  return input;
}

ErosionParams erosion_preset(const MountainParams &p)
{
  ErosionParams e;
  e.seed = p.seed;
  e.world_width = p.world_width;
  e.world_height = p.world_height;
  e.suspended_amount = 1.f;
  e.suspended_angle = 24.f;
  e.bed_load_angle = 15.f;
  e.coarse_angle = 10.f;

  switch (p.style)
  {
  case MountainStyle::OLD:
    e.duration = 40.f;
    e.downcutting = .1f;
    e.erosion_scale = 1600.f;
    e.bed_load_amount = 0.f;
    e.coarse_amount = .65f;
    e.shape = .5f;
    e.shape_sharpness = .5f;
    e.shape_detail_scale = p.reduce_details ? .05f : .25f;
    break;
  case MountainStyle::ALPINE:
  case MountainStyle::STRATA:
    e.duration = 60.f;
    e.downcutting = .5f;
    e.erosion_scale = 1200.f;
    e.bed_load_amount = .33f;
    e.coarse_amount = 0.f;
    e.shape = .8f;
    e.shape_sharpness = .9f;
    e.shape_detail_scale = 0.f;
    break;
  default: // ERODED
    e.duration = 10.f;
    e.downcutting = .15f;
    e.erosion_scale = 1400.f;
    e.bed_load_amount = 0.f;
    e.coarse_amount = 0.f;
    e.shape = .8f;
    e.shape_sharpness = p.reduce_details ? .3f : .6f;
    e.shape_detail_scale = p.reduce_details ? .05f : .4f;
    break;
  }
  return e;
}

} // namespace

Field mountain(int n, const MountainParams &p)
{
  // three seeded ridge layers, random draws in the recovered order
  Rng   rng(p.seed);
  float sa = rng.range(.5f, .75f);
  Field a = ridge(n, sa, .6f, .4f, p.seed);
  float sb = rng.range(.65f, .95f), db = rng.range(.5f, 1.f);
  Field b = ridge(n, sb, .6f, db, p.seed + 1);
  float sc = rng.range(.35f, .7f), dc = rng.range(.1f, .6f);
  Field c = ridge(n, sc, .6f, dc, p.seed + 2);

  // smoothstep radial envelope
  float cx = float(n) * p.x, cy = float(n) * p.y;
  float radius = std::max(float(n) * p.scale, 1e-6f);

  parallel_rows(n,
                [&](int y)
                {
                  for (int x = 0; x < n; x++)
                  {
                    float dx = float(x) - cx, dy = float(y) - cy;
                    float q = std::max(1.f - std::sqrt(dx * dx + dy * dy) / radius, 0.f);
                    q = q * q * (3.f - 2.f * q);
                    size_t i = size_t(y) * n + x;
                    a.data[i] = (a.data[i] + b.data[i] + c.data[i]) * q;
                  }
                });

  if (p.style != MountainStyle::ALPINE)
    a = combine(a, fractal_warp(a, .5f, .5f, 0, .5f, p.seed + 36, 0), false);

  for (float &h : a.data)
    h *= p.height;

  if (p.style != MountainStyle::BASIC)
  {
    a = erode(std::move(a), erosion_preset(p)).height;

    if (p.style == MountainStyle::ALPINE || p.style == MountainStyle::STRATA)
      for (float &h : a.data)
        h *= 2.f;

    if (p.style == MountainStyle::STRATA)
      a = strata(std::move(a), p.seed);
  }

  if (p.bulk == MountainBulk::LOW)
    bulk_adjustment(a, -.25f);
  else if (p.bulk == MountainBulk::HIGH)
    bulk_adjustment(a, .2f);

  for (float &v : a.data)
    v = clamp01(v);

  return a;
}

} // namespace hesiod::landscape
