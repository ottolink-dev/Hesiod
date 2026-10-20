/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

// Scalar translation of the unmasked and masked ParallelErosion CPU path
// (controller 215e60, level 214c60, init 2158b0 / 215ab0, trace 213d70,
// update 2145d0, shape 217100, downsample 591d0, reconstruct 58610). Particle
// passes are kept sequential so a seed always gives the same terrain.
#include <array>
#include <cmath>
#include <limits>
#include <random>

#include "hesiod/model/nodes/landscape/erosion.hpp"

#include "common.hpp"

namespace hesiod::landscape
{

using namespace detail;

namespace
{

struct State
{
  ErosionParams p;
  float         octave_count = 0.f;
  float         detail_octave = 0.f;
  float         tan_suspended = 0.f;
  float         tan_mixed = 0.f;
  float         tan_coarse = 0.f;
  float         local_downcut = 0.f;
  float         local_shape = 0.f;
  float         spatial_weight = 1.f;
};

float level_weight(int   level,
                   float octaves,
                   float start,
                   float smoothing,
                   float base,
                   bool  fractional)
{
  float o = octaves;
  if (o < 1.f)
    o = std::exp(o - 1.f);
  float fraction = o - std::floor(o);
  if (fraction == 0.f)
    fraction = 1.f;
  float x = std::max(0.f, float(level) - (1.f - fraction) - start);
  float integrated = std::exp(-x * smoothing) / smoothing + x - 1.f / smoothing;
  return std::exp(integrated * std::log(base)) *
         ((level <= 0 && fractional) ? fraction : 1.f);
}

// separable [1 3 3 1] / 64, clamped
Field downsample(const Field &g)
{
  Field     o(g.n / 2);
  const int w[] = {1, 3, 3, 1};
  parallel_rows(o.n,
                [&](int y)
                {
                  for (int x = 0; x < o.n; x++)
                  {
                    float sum = 0.f;
                    for (int j = 0; j < 4; j++)
                      for (int i = 0; i < 4; i++)
                        sum += edge(g, x * 2 + i - 1, y * 2 + j - 1) * float(w[i] * w[j]);
                    o.at(x, y) = sum * .015625f;
                  }
                });
  return o;
}

// parity-selected 9 3 3 1 / 16
float upsample_at(const Field &g, int x, int y)
{
  int ix = x >> 1, iy = y >> 1;
  int nx = std::clamp(ix + ((x & 1) * 2 - 1), 0, g.n - 1);
  int ny = std::clamp(iy + ((y & 1) * 2 - 1), 0, g.n - 1);
  return (g.at(ix, iy) * 9.f + g.at(nx, iy) * 3.f + g.at(ix, ny) * 3.f + g.at(nx, ny)) *
         .0625f;
}

struct Layer
{
  Field                h, flow, wear, deposits, residual;
  Field                mask; // empty when unmasked
  std::vector<int32_t> touches, sums, weights;

  explicit Layer(Field height)
      : h(std::move(height)), flow(h.n), wear(h.n), deposits(h.n), residual(h.n),
        touches(h.data.size()), sums(h.data.size()), weights(h.data.size())
  {
  }
};

struct Particle
{
  std::array<float, 12> height{}, uphill{}, deposit{};
  std::array<V2, 12>    path{};
  V2                    pos{};
  int                   count = 0, total = 0, uphill_steps = 0, noise_index = 0;
  bool                  terminated = false, dead = false;
  float                 previous = -999999.875f, previous2 = 0.f, third = 0.f;
  float                 last_delta = 0.f, carried = 0.f, coarse_carried = 0.f;
};

bool sample_height(const Field &g, V2 pos, float &height, V2 &gradient)
{
  int x = int(pos.x - .5f), y = int(pos.y - .5f);
  if (x < 0 || y < 0 || pos.x >= float(g.n) - 1.5f || pos.y >= float(g.n) - 1.5f)
    return false;

  float tx = pos.x - float(x) - .5f, ty = pos.y - float(y) - .5f;
  float a = g.at(x, y), b = g.at(x + 1, y), c = g.at(x, y + 1), d = g.at(x + 1, y + 1);
  height = ((1.f - tx) * a + tx * b) * (1.f - ty) + (tx * d + (1.f - tx) * c) * ty;
  gradient = {(b - a) * (1.f - ty) + (d - c) * ty, (d - b) * tx + (c - a) * (1.f - tx)};
  return true;
}

V2 unit_down(V2 g)
{
  float d = std::sqrt(g.x * g.x + g.y * g.y);
  return d > 0.f ? V2{-g.x / d, -g.y / d} : V2{};
}

void trace(Layer &l, Particle &p, const std::array<V2, 32> &directions)
{
  p.count = 0;
  float h;
  V2    grad;
  if (!sample_height(l.h, p.pos, h, grad) || grad.x * grad.x + grad.y * grad.y <= 0.f)
  {
    p.dead = true;
    return;
  }

  V2   direction = unit_down(grad), previous_direction = direction;
  V2   pos = p.pos, last_valid = pos;
  bool uphill = false, outside = false, stop = false;

  if (!(p.previous > -999999.f))
    p.previous = h;
  p.previous2 = p.previous;
  p.third = p.previous;

  int count = 0;
  for (int j = 0; j < 11; j++)
  {
    last_valid = pos;
    p.path[j] = pos;
    p.height[j] = h;
    p.uphill[j] = 0.f;

    size_t ix = size_t(int(pos.y)) * l.h.n + int(pos.x);
    l.flow.data[ix] += 1.f;
    l.touches[ix]++;
    p.deposit[j] = l.deposits.data[ix];

    float old_h = h;
    pos = pos + direction;
    V2 new_grad;

    if (!sample_height(l.h, pos, h, new_grad))
    {
      p.dead = true;
      outside = true;
      stop = true;
    }
    else
    {
      p.dead = false;
      p.noise_index = (p.noise_index + 1) & 31;

      float length = std::sqrt(new_grad.x * new_grad.x + new_grad.y * new_grad.y);
      if (length <= 0.f)
      {
        direction = {0.f, 0.f};
        stop = true;
      }
      else
      {
        float inertia = uphill ? 30.f : 1.f;
        V2 d = previous_direction * (.5f * inertia) + directions[p.noise_index] * .25f +
               unit_down(new_grad);
        float r = std::sqrt(d.x * d.x + d.y * d.y);
        direction = d * (1.f / r);
      }
      previous_direction = direction;

      if (old_h <= h)
      {
        p.uphill_steps++;
        uphill = true;
        p.uphill[j] = h - old_h;
        p.height[j] = h;
        if (!(p.uphill_steps < 5 && p.total + j > 2))
        {
          stop = true;
          p.terminated = true;
        }
      }
      else
        uphill = false;
    }

    count = j + 1;
    if (stop)
      break;
  }

  if (uphill)
    p.terminated = true;

  if (p.terminated || outside)
    p.pos = last_valid;
  else
  {
    p.path[count] = pos;
    p.height[count] = h;
    p.uphill[count] = 0.f;
    p.pos = pos;

    size_t ix = size_t(int(pos.y)) * l.h.n + int(pos.x);
    l.touches[ix]++;
    l.flow.data[ix] += 1.f;
    p.deposit[count] = l.deposits.data[ix];
    count++;
  }

  p.total += count;
  p.count = count;
}

int32_t wrapped_add(int32_t a, int32_t b) { return int32_t(uint32_t(a) + uint32_t(b)); }

void apply_cell(Layer &l, size_t ix)
{
  if (l.weights[ix] > 0)
  {
    float change = float(l.sums[ix]) * 5.960464477539063e-8f / float(l.weights[ix]);
    l.h.data[ix] += change;
    l.deposits.data[ix] = std::max(0.f,
                                   l.deposits.data[ix] +
                                       (change <= 0.f ? change * .95f : change));
    l.wear.data[ix] += -std::min(0.f, change);
  }
  l.sums[ix] = l.weights[ix] = 0;
}

void update(Layer &l, Particle &p, const State &s)
{
  int count = p.count;
  if (count <= 0)
    return;

  float last = p.height[count - 1];
  float cell = (s.p.world_width / s.p.world_height) / float(l.h.n);

  for (int j = 0; j < count; j++)
  {
    size_t ix = size_t(int(p.path[j].y)) * l.h.n + int(p.path[j].x);

    float prev = p.height[std::max(0, j - 1)];
    float prev2 = p.height[std::max(0, j - 2)];
    float next = p.height[std::min(count - 1, j + 1)];
    float next2 = p.height[std::min(count - 1, j + 2)];
    float curr = p.height[j];

    float slope = (prev - next) * .5f / cell, positive_slope = std::max(0.f, slope);
    float modifier = 1.f;
    if (s.tan_suspended <= positive_slope * s.p.suspended_amount)
      modifier = .5f / (positive_slope * 4.f + 1.f);

    float mean3 = (prev + curr + next) / 3.f;
    float mean5 = (prev + prev2 + curr + next + next2) / 5.f;
    float d3 = mean3 - curr, d5 = mean5 - curr;
    float positive = std::max(0.f, std::max(d3, d5)), candidate = positive * modifier;
    float avg = (d3 >= 0.f || d5 >= 0.f) ? mean3 : mean5;

    // DowncuttingType = 0
    float erosion = (std::min(curr, avg) - (next2 * .95f + last * .05f)) *
                    s.local_downcut;
    float carry = p.carried;

    if (erosion + positive < carry)
    {
      if (s.tan_coarse * .8f < slope && slope < s.tan_coarse * 1.25f)
        candidate += std::min(carry * .5f * s.p.coarse_amount * .5f, erosion);
      if (slope < s.tan_mixed)
      {
        float factor = positive_slope < s.tan_mixed * .1f ? 100.f : 1.f;
        candidate += std::min(carry * .3f * s.p.bed_load_amount * .5f * factor, erosion);
      }
    }

    float target = std::min(curr, avg) - erosion;
    float deposited = std::min(std::min((p.previous - target) * .9f, candidate), carry);
    // RockSoftness / SedimentTransportCapacity are zeroed by the caller, which
    // switches the coarse deposit term off
    float coarse_deposit = std::min((p.previous2 - (target + deposited)) * .95f,
                                    p.coarse_carried * .02f * 0.f * l.flow.data[ix]);

    target += deposited + coarse_deposit;
    p.coarse_carried -= coarse_deposit;
    p.last_delta = p.previous - target;
    p.previous2 = p.previous;
    p.previous = target;

    float delta = target - curr;
    p.carried = std::max(0.f, carry - (delta >= 0.f ? delta * 1.05f : delta));

    int weight = (j == 0 || j == count - 1 || p.terminated) ? 1 : 2;
    delta *= float(weight * weight) * .5f * (p.terminated ? 0.f : 1.f) *
             (p.total > 3 ? 1.f : 0.f);
    if (p.uphill[j] > 0.f)
      delta = std::max(p.uphill[j], delta);

    // 24-bit fixed point aggregation, saturated instead of failing
    double scaled = double(delta * 16777216.f);
    if (!std::isfinite(scaled))
      scaled = 0.;
    scaled = std::clamp(scaled, -2147483648., 2147483647.);

    l.sums[ix] = wrapped_add(l.sums[ix], int32_t(scaled));
    l.weights[ix] += weight;

    if (--l.touches[ix] == 0)
      apply_cell(l, ix);
  }
}

void shape(Layer &l, const State &s)
{
  int   n = l.h.n;
  Field delta(n);
  float cell = (s.p.world_width / s.p.world_height) / float(n);
  float sharp = s.p.shape_sharpness;

  parallel_rows(
      n,
      [&](int y)
      {
        for (int x = 0; x < n; x++)
        {
          float v[9];
          for (int j = -1; j <= 1; j++)
            for (int i = -1; i <= 1; i++)
              v[(j + 1) * 3 + i + 1] = edge(l.h, x + i, y + j);

          float center = v[4], sum = 0.f, weighted = 0.f;
          for (int i = 0; i < 9; i++)
            if (i != 4 && v[i] < center)
            {
              float d = center - v[i], w = d / cell;
              sum += w;
              weighted -= w * d;
            }
          if (sum > 0.f)
            weighted /= sum;

          float gx = ((v[5] + v[2] + v[5] + v[8]) - (v[3] + v[0] + v[3] + v[6])) * .125f;
          float gy = ((v[7] + v[6] + v[7] + v[8]) - (v[1] + v[0] + v[1] + v[2])) * .125f;
          float blend = sharp * sharp * .9f;
          float gradient = (std::sqrt(gx * gx + gy * gy) * (1.f - blend) -
                            blend * weighted) *
                           sharp;
          float a = std::max(0.f, gradient - cell * .3f);
          float b = std::max(0.f, gradient - cell);
          float highpass = center - ((v[2] + v[0] + v[6] + v[8]) * .7071f + v[3] + v[1] +
                                     v[5] + v[7]) *
                                        .1464472f;
          float force = std::max(
              weighted,
              -(a * .7f + b * 30.f + (1.f - sharp) * .02f * s.spatial_weight * highpass));
          delta.at(x, y) = force * s.local_shape;
        }
      });

  for (size_t i = 0; i < delta.data.size(); i++)
  {
    l.h.data[i] += delta.data[i];
    l.deposits.data[i] += delta.data[i];
  }
}

// 32 unit directions, shuffled with a portable Fisher-Yates so every platform
// gets the same table for a given seed
std::array<V2, 32> random_directions(int seed)
{
  std::array<V2, 32> d;
  for (int i = 0; i < 32; i++)
  {
    float a = float(i) * (6.2831855f / 32.f);
    d[i] = {std::cos(a), std::sin(a)};
  }

  std::mt19937 engine{uint32_t(seed)};
  for (int i = 31; i > 0; i--)
    std::swap(d[i], d[engine() % uint32_t(i + 1)]);
  return d;
}

uint32_t xor32(uint32_t v)
{
  v ^= v << 13;
  v ^= v >> 17;
  v ^= v << 5;
  return v;
}

float unit_float(uint32_t v) { return float(v * 0x2545f381u) * 2.3283064e-10f; }

void simulate_level(Layer &l, State &s, int level, const std::array<V2, 32> &directions)
{
  int n = l.h.n;
  int iterations = int(std::ceil(
      level_weight(level, s.octave_count, 2.5f, .75f, .5f, true) * s.p.duration));

  s.local_downcut = level_weight(level, s.octave_count, 1.f, 1.2f, .6f, false) *
                    s.p.downcutting;
  s.local_shape = level_weight(level, s.octave_count, s.detail_octave, 10.f, .4f, false) *
                  s.p.shape;
  s.spatial_weight = std::max(1.f,
                              level_weight(level, s.octave_count, 0.f, 10.f, 2.f, false));

  int                   particles = n * n / 256;
  std::vector<Particle> pool(particles);
  std::vector<int>      active, next;
  active.reserve(particles);
  next.reserve(particles);

  bool     masked = l.mask.n == n;
  uint32_t offset = 0;
  uint32_t useed = uint32_t(s.p.seed);

  for (int it = 0; it < iterations; it++)
  {
    active.clear();
    for (int i = 0; i < particles; i++)
    {
      uint32_t index = offset + uint32_t(i);
      uint32_t seed = useed * 0x1b227u + index;
      if (!seed)
        seed = 0xa2351342u;
      uint32_t a = xor32(seed), b = xor32(a);

      Particle p;
      p.pos = {unit_float(a) * float(n), unit_float(b) * float(n)};
      p.noise_index = int(offset & 31u);

      if (masked)
      {
        // 215ab0: keep the particle only if a second draw falls below the mask
        uint32_t aseed = useed * 0x1b253u + index;
        if (!aseed)
          aseed = 0xa2351342u;
        int mx = std::clamp(int(p.pos.x), 0, n - 1);
        int my = std::clamp(int(p.pos.y), 0, n - 1);
        if (!(unit_float(xor32(aseed)) < l.mask.at(mx, my)))
          continue;
      }

      pool[i] = p;
      active.push_back(i);
    }

    int batches = 0;
    while (!active.empty() && ++batches <= n * 8)
    {
      next.clear();
      for (int i : active)
      {
        trace(l, pool[i], directions);
        if (pool[i].terminated)
          pool[i].dead = true;
        if (!pool[i].dead)
          next.push_back(i);
      }
      for (int i : active)
        update(l, pool[i], s);
      active.swap(next);
    }

    if (s.p.shape > 0.f)
      shape(l, s);
    offset += uint32_t(particles);
  }

  // settle anything left behind by an interrupted pass
  for (size_t ix = 0; ix < l.touches.size(); ix++)
    if (l.touches[ix] != 0)
    {
      apply_cell(l, ix);
      l.touches[ix] = 0;
    }
}

void reconstruct(Layer &fine, const Layer &coarse, const State &s)
{
  float cell = (s.p.world_width / s.p.world_height) / float(fine.h.n);

  parallel_rows(fine.h.n,
                [&](int y)
                {
                  for (int x = 0; x < fine.h.n; x++)
                  {
                    float h = upsample_at(coarse.h, x, y);
                    float res = fine.residual.at(x, y);
                    float dep = upsample_at(coarse.deposits, x, y);
                    float wear = upsample_at(coarse.wear, x, y);

                    if (dep <= 0.f)
                      h += res;
                    else if (dep - res > 0.f)
                    {
                      if (res > 0.f)
                      {
                        float d = std::min((cell + cell) / dep - .25f, 1.f) * res;
                        h += d;
                        dep -= d;
                      }
                    }
                    else
                    {
                      float f = std::max(.1f, std::min(1.f, cell / wear));
                      h += f * (res - dep);
                      dep = 0.f;
                    }

                    fine.h.at(x, y) = h;
                    fine.deposits.at(x, y) = dep;
                    fine.wear.at(x, y) = std::max(0.f, wear - res);
                  }
                });
}

// Scharr-style slope in degrees, Gaea AspectMaps.GetSlope
Field slope_degrees(const Field &h, float world_width, float world_height)
{
  int   n = h.n;
  Field out(n);
  float c = (1.f / 32.f) * world_height / (world_width / float(n));

  parallel_rows(n,
                [&](int y)
                {
                  for (int x = 0; x < n; x++)
                  {
                    float nw = edge(h, x - 1, y - 1), nn = edge(h, x, y - 1);
                    float ne = edge(h, x + 1, y - 1), ww = edge(h, x - 1, y);
                    float ee = edge(h, x + 1, y), sw = edge(h, x - 1, y + 1);
                    float ss = edge(h, x, y + 1), se = edge(h, x + 1, y + 1);
                    float gx = 3.f * (ne + se - nw - sw) + 10.f * (ee - ww);
                    float gy = 3.f * (nw + ne - sw - se) + 10.f * (nn - ss);
                    out.at(x, y) = std::atan(std::sqrt(gx * gx + gy * gy) * c) *
                                   57.295780181884766f;
                  }
                });
  return out;
}

// Directional rain shadow. Gaea feeds Lighting2 with azimuth = Direction and
// elevation = 80 * (1 - q), softness 20 * (1 - q), q = RainShadow * (2 -
// RainShadow); Lighting2 itself was not recovered, so this is a horizon-angle
// cast shadow with the same inputs.
Field rain_shadow(const Field &h, const PrecipitationParams &p)
{
  int   n = h.n;
  float q = p.rain_shadow * (2.f - p.rain_shadow);
  float elevation = 80.f * (1.f - q) * PI / 180.f;
  float softness = 20.f * (1.f - q) * PI / 180.f;
  float cell = p.world_width / float(n); // meters per pixel
  float vertical = p.world_height;       // meters per height unit

  float dir = p.direction * PI / 180.f;
  float lx = std::cos(dir), ly = std::sin(dir);

  // sample distances: dense close to the pixel, then geometric
  std::vector<float> dist;
  for (float d = 1.f; d < float(n); d = d < 16.f ? d + 1.f : d * 1.08f)
    dist.push_back(d);

  Field out(n);
  parallel_rows(n,
                [&](int y)
                {
                  for (int x = 0; x < n; x++)
                  {
                    float h0 = h.at(x, y);
                    float horizon = -PI * .5f;
                    for (float d : dist)
                    {
                      float sx = float(x) + lx * d, sy = float(y) + ly * d;
                      if (sx < 0.f || sy < 0.f || sx > float(n - 1) || sy > float(n - 1))
                        break;
                      float hs = bilinear(h.data, n, sx, sy, 0);
                      horizon = std::max(horizon,
                                         std::atan2((hs - h0) * vertical, d * cell));
                    }

                    float lit;
                    if (softness > 0.f)
                      lit = clamp01((elevation - horizon) / softness + .5f);
                    else
                      lit = horizon < elevation ? 1.f : 0.f;
                    out.at(x, y) = lit;
                  }
                });
  return out;
}

} // namespace

ErosionResult erode(Field input, const ErosionParams &params, const Field *p_mask)
{
  State s;
  s.p = params;
  s.p.world_width = std::max(params.world_width, 1e-3f);
  s.p.world_height = std::max(params.world_height, 1e-3f);

  int n0 = input.n;

  // soft-limit the effective scale above 20% of the terrain width
  float r = s.p.erosion_scale / s.p.world_width;
  if (r > .2f)
    r = (1.f - 1.f / ((r - .2f) * 6.f + 1.f)) * .16666667f + .2f;
  float scale = s.p.world_width * r;

  s.octave_count = std::log2((scale * .5f) / (s.p.world_width / float(n0)));
  if (!std::isfinite(s.octave_count))
    s.octave_count = 0.f;
  s.detail_octave = s.p.shape_detail_scale == 0.f
                        ? 100.f
                        : std::log2(1.f / s.p.shape_detail_scale);
  s.tan_suspended = std::tan(s.p.suspended_angle / 180.f * 3.1415927f);
  s.tan_mixed = std::tan(s.p.bed_load_angle / 180.f * 3.1415927f);
  s.tan_coarse = std::tan(s.p.coarse_angle / 180.f * 3.1415927f);

  // pyramid depth, never below 4 x 4 at the coarsest level
  int depth = std::max(1, int(std::ceil(s.octave_count)));
  while (depth > 1 && (n0 >> (depth - 1)) < 4)
    depth--;

  std::vector<Layer> levels;
  levels.reserve(depth);
  levels.emplace_back(std::move(input));
  if (p_mask && p_mask->n == n0)
    levels.back().mask = *p_mask;

  for (int j = 1; j < depth; j++)
  {
    Layer &fine = levels.back();
    Field  coarse = downsample(fine.h);

    for (int y = 0; y < fine.h.n; y++)
      for (int x = 0; x < fine.h.n; x++)
        fine.residual.at(x, y) = fine.h.at(x, y) - upsample_at(coarse, x, y);

    Field coarse_mask;
    if (fine.mask.n == fine.h.n)
      coarse_mask = downsample(fine.mask);

    levels.emplace_back(std::move(coarse));
    levels.back().mask = std::move(coarse_mask);
  }

  // coarse to fine
  auto dirs = random_directions(s.p.seed);
  int  level = 0;
  for (int j = int(levels.size()) - 1; j >= 0; j--)
  {
    simulate_level(levels[j], s, level++, dirs);
    if (j > 0)
      reconstruct(levels[j - 1], levels[j], s);
  }

  ErosionResult result;
  result.height = std::move(levels[0].h);
  result.flow = std::move(levels[0].flow);
  result.deposits = std::move(levels[0].deposits);
  result.wear = std::move(levels[0].wear);
  return result;
}

Field precipitation_mask(const Field &height, const PrecipitationParams &p)
{
  int   n = height.n;
  Field slope = slope_degrees(height, p.world_width, p.world_height);
  Field alt = height;
  normalize(alt);

  Field mask(n);
  for (size_t i = 0; i < mask.data.size(); i++)
  {
    float sv = slope.data[i], av = alt.data[i];
    bool  in_slope = p.slope_min <= sv && sv <= p.slope_max;
    bool  in_alt = p.altitude_min <= av && av <= p.altitude_max;
    mask.data[i] = (in_slope && in_alt) ? 1.f : 0.f;
  }

  if (p.directional)
  {
    Field shadow = rain_shadow(height, p);
    for (size_t i = 0; i < mask.data.size(); i++)
      mask.data[i] *= shadow.data[i];
  }

  if (p.reverse)
    for (float &v : mask.data)
      v = 1.f - v;

  return mask;
}

} // namespace hesiod::landscape
