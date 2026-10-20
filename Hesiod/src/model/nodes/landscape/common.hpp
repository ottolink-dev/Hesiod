/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

// Helpers shared by the landscape simulations. Arithmetic order follows the
// recovered code on purpose (float32 everywhere, same RNG draws), so keep
// "simplifications" out of here unless they are exact.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

#include "hesiod/model/nodes/landscape/field.hpp"

namespace hesiod::landscape::detail
{

constexpr float PI = 3.14159265358979323846f;

inline float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }
inline float mix(float a, float b, float t) { return a + (b - a) * t; }

struct V2
{
  float x = 0.f;
  float y = 0.f;

  V2 operator+(V2 b) const { return {x + b.x, y + b.y}; }
  V2 operator-(V2 b) const { return {x - b.x, y - b.y}; }
  V2 operator*(float s) const { return {x * s, y * s}; }
};

inline V2 mix(V2 a, V2 b, float t) { return a + (b - a) * t; }

inline float edge(const Field &g, int x, int y)
{
  return g.at(std::clamp(x, 0, g.n - 1), std::clamp(y, 0, g.n - 1));
}

// run fct(row) for every row, rows are independent so the result does not depend
// on the thread count
template <class F> void parallel_rows(int rows, F &&fct)
{
  unsigned nt = std::max(1u, std::thread::hardware_concurrency());
  nt = std::min<unsigned>(nt, unsigned(std::max(rows, 1)));

  if (nt <= 1 || rows < 32)
  {
    for (int y = 0; y < rows; y++)
      fct(y);
    return;
  }

  std::atomic<int>         next{0};
  std::vector<std::thread> pool;
  pool.reserve(nt);
  for (unsigned t = 0; t < nt; t++)
    pool.emplace_back(
        [&]()
        {
          for (int y = next.fetch_add(1); y < rows; y = next.fetch_add(1))
            fct(y);
        });
  for (auto &th : pool)
    th.join();
}

// Gaea's XorShift64* (managed side, drives the per-layer random choices)
struct Rng
{
  uint64_t s;

  explicit Rng(int seed)
      : s(seed ? uint64_t(int64_t(seed)) : uint64_t(-6758474490012166922LL))
  {
  }

  uint64_t step()
  {
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 7;
    return s * 2685821657736338717ULL;
  }

  float next() { return float(step() >> 24) * 9.094947017729282e-13f; }
  float range(float a, float b) { return a + next() * (b - a); }
  int   integer() { return int(step() >> 33); }
  int   rangei(int a, int b) { return a + int((step() - 1) & 2147483647ULL) % (b - a); }
};

// edge = 0 clamps, anything else mirrors
inline int boundary(int v, int n, int edge_mode)
{
  if (!edge_mode)
    return std::clamp(v, 0, n - 1);
  int q = std::abs(v) % (n * 2);
  return q < n ? q : n * 2 - 1 - q;
}

// fractional weights are taken before the boundary transform (matches Gaea for
// negative coordinates)
template <class T>
T bilinear(const std::vector<T> &data, int n, float x, float y, int edge_mode)
{
  int   ix = int(x), iy = int(y);
  float tx = x - float(ix), ty = y - float(iy);
  int   x0 = boundary(ix, n, edge_mode), x1 = boundary(ix + 1, n, edge_mode);
  int   y0 = boundary(iy, n, edge_mode), y1 = boundary(iy + 1, n, edge_mode);
  return mix(mix(data[size_t(y0) * n + x0], data[size_t(y0) * n + x1], tx),
             mix(data[size_t(y1) * n + x0], data[size_t(y1) * n + x1], tx),
             ty);
}

inline Field combine(const Field &a, const Field &b, bool maximum)
{
  Field o(a.n);
  for (size_t i = 0; i < a.data.size(); i++)
    o.data[i] = maximum ? std::max(a.data[i], b.data[i]) : std::min(a.data[i], b.data[i]);
  return o;
}

// clamp(x - min(1, min_sample), 0, 1)
inline void range_shift_clamp(Field &g)
{
  float lo = 1.f;
  for (float v : g.data)
    lo = std::min(lo, v);
  for (float &v : g.data)
    v = clamp01(v - lo);
}

// affine extrema remap to [0, 1], flat fields map to 0
inline void normalize(Field &g)
{
  auto  mm = std::minmax_element(g.data.begin(), g.data.end());
  float lo = *mm.first, r = *mm.second - lo;
  if (r == 0.f)
  {
    std::fill(g.data.begin(), g.data.end(), 0.f);
    return;
  }
  for (float &v : g.data)
    v = (v - lo) / r;
}

} // namespace hesiod::landscape::detail
