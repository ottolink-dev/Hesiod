/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

// FastNoiseSIMD sampling and Gaea-style fractal warps used by the mountain
// generator.
#pragma once
#include <memory>

#include "FastNoiseSIMD.h"

#include "common.hpp"

namespace hesiod::landscape::detail
{

using Noise = std::unique_ptr<FastNoiseSIMD>;

inline Noise make_noise(int seed) { return Noise(FastNoiseSIMD::NewFastNoiseSIMD(seed)); }

// sample a noise object on the n x n grid, optionally centered on the map
inline Field noise_map(int n, FastNoiseSIMD &fn, bool centered, bool to01)
{
  Field g(n);
  float c = centered ? float(n) * .5f : 0.f;

  parallel_rows(n,
                [&](int y)
                {
                  FastNoiseVectorSet vectors(n);
                  vectors.sampleScale = 0;
                  vectors.sampleSizeX = n;
                  vectors.sampleSizeY = 1;
                  vectors.sampleSizeZ = 1;
                  for (int x = 0; x < n; x++)
                  {
                    vectors.xSet[x] = float(x) - c;
                    vectors.ySet[x] = float(y) - c;
                    vectors.zSet[x] = 0.f;
                  }

                  float *row = FastNoiseSIMD::GetEmptySet(n);
                  fn.FillSampledNoiseSet(row, &vectors, 0.f, 0.f, 0.f);
                  for (int x = 0; x < n; x++)
                    g.at(x, y) = to01 ? .5f + row[x] * .5f : row[x];
                  FastNoiseSIMD::FreeNoiseSet(row);
                });

  return g;
}

inline Field perlin(int n, float scale, int seed, int octaves = 12, float gain = .5f)
{
  auto f = make_noise(seed);
  f->SetFrequency((1.f - scale) * .01f * (512.f / float(n)));
  f->SetAxisScales(1.f, 1.f, 1.f);
  f->SetFractalOctaves(octaves);
  f->SetNoiseType(FastNoiseSIMD::PerlinFractal);
  f->SetFractalGain(gain);
  f->SetFractalType(FastNoiseSIMD::FBM);
  f->SetPerturbType(FastNoiseSIMD::None);
  return noise_map(n, *f, true, true);
}

inline Field voronoi(int n, float scale, int seed)
{
  auto f = make_noise(seed);
  f->SetFrequency(scale * .01f * (512.f / float(n)));
  f->SetCellularNoiseLookupFrequency(scale * (512.f / float(n)));
  f->SetAxisScales(1.f, 1.f, 1.f);
  f->SetCellularNoiseLookupType(FastNoiseSIMD::PerlinFractal);
  f->SetNoiseType(FastNoiseSIMD::Cellular);
  f->SetCellularJitter(.45f);
  f->SetCellularDistanceFunction(FastNoiseSIMD::Euclidean);
  f->SetCellularReturnType(FastNoiseSIMD::Distance2);
  f->SetPerturbType(FastNoiseSIMD::GradientFractal);
  f->SetPerturbAmp(3.f);
  f->SetPerturbFrequency(.1f);
  f->SetPerturbFractalOctaves(10);
  return noise_map(n, *f, true, true);
}

// Gaea FractalWarp, selected Mountain paths only: one requested iteration,
// zCoeff = 0, no modulator, no normalized warp. Cellular kinds add a final
// Perlin pass when perturb != 0.
inline Field fractal_warp(const Field &g,
                          float        size,
                          float        strength,
                          int          kind,
                          float        perturb,
                          int          seed,
                          int          edge_mode)
{
  int n = g.n;
  size = std::max(.0001f, size);
  float freq = 1.f / (size * float(n));
  float warp = strength * float(n) * size;

  auto nx = make_noise(1337), ny = make_noise(1337);
  for (auto *f : {nx.get(), ny.get()})
  {
    f->SetFractalOctaves(12);
    f->SetFractalGain(.5f);
    f->SetFrequency(freq);
    if (kind == 0)
    {
      f->SetNoiseType(FastNoiseSIMD::PerlinFractal);
      f->SetFractalType(FastNoiseSIMD::FBM);
    }
    else
    {
      f->SetNoiseType(FastNoiseSIMD::Cellular);
      f->SetCellularReturnType(FastNoiseSIMD::Distance2Div);
    }
  }
  if (kind != 0)
    nx->SetCellularJitter(.45f);

  std::vector<V2> field(size_t(n) * n), next(field.size());
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      field[size_t(y) * n + x] = {float(x), float(y)};

  int iterations = (kind != 0 && perturb != 0.f) ? 2 : 1;

  for (int k = 0; k < iterations; k++)
  {
    if (k == iterations - 1 && iterations == 2)
    {
      for (auto *f : {nx.get(), ny.get()})
      {
        f->SetNoiseType(FastNoiseSIMD::PerlinFractal);
        f->SetFractalType(FastNoiseSIMD::FBM);
        f->SetFrequency(freq * 4.f);
      }
      warp = perturb * float(n) * size * .25f;
    }

    nx->SetSeed(seed++);
    ny->SetSeed(seed++);
    Field dx = noise_map(n, *nx, false, false);
    Field dy = noise_map(n, *ny, false, false);
    float sign = (k & 1) ? -1.f : 1.f;
    if (iterations == 2 && k == 1)
      sign = 1.f;

    parallel_rows(n,
                  [&](int y)
                  {
                    for (int x = 0; x < n; x++)
                    {
                      size_t i = size_t(y) * n + x;
                      next[i] = bilinear(field,
                                         n,
                                         float(x) + dx.data[i] * sign * warp,
                                         float(y) + dy.data[i] * sign * warp,
                                         edge_mode);
                    }
                  });
    field.swap(next);
  }

  Field out(n);
  parallel_rows(n,
                [&](int y)
                {
                  for (int x = 0; x < n; x++)
                  {
                    size_t i = size_t(y) * n + x;
                    out.data[i] = bilinear(g.data, n, field[i].x, field[i].y, edge_mode);
                  }
                });
  return out;
}

} // namespace hesiod::landscape::detail
