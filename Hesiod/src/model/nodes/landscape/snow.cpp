/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

// Scalar port of the snow path of Gaea.CS.dll (Erosion1, wrapper version 1):
// entry 4b10, pyramid 4540, simulation 3bd0, avalanche step 1320, accumulate
// step 3660, masks 37e0, final 2170, Gaussian down/up-sampling 62b0/6850.
// Float/double mixing follows the recovered code. Rows of the avalanche step
// run in 4 interleaved phases, which is what makes it race-free and
// deterministic in parallel (the original orders its rows the same way).
#include <array>
#include <cmath>

#include "hesiod/model/nodes/landscape/snow.hpp"

#include "common.hpp"

namespace hesiod::landscape
{

using namespace detail;

namespace
{

// 5x5 Gaussian, Gaea.CS.dll 0x18000e550 (copied into globals by the Erosion1
// constructor)
constexpr std::array<float, 25> KERNEL = {
    0.003765f, 0.015019f, 0.023792f, 0.015019f, 0.003765f, //
    0.015019f, 0.059912f, 0.094907f, 0.059912f, 0.015019f, //
    0.023792f, 0.094907f, 0.150342f, 0.094907f, 0.023792f, //
    0.015019f, 0.059912f, 0.094907f, 0.059912f, 0.015019f, //
    0.003765f, 0.015019f, 0.023792f, 0.015019f, 0.003765f};

// neighbor order and threshold weights, 0x18000e4e8 / e528 / e508
constexpr int   DX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
constexpr int   DY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
constexpr float WN[8] = {0.707f, 1.f, 0.707f, 1.f, 1.f, 0.707f, 1.f, 0.707f};

float kernel_sum()
{
  float s = 0.f;
  for (float k : KERNEL)
    s += k;
  return s;
}

Field downsample(const Field &fine)
{
  Field       c(fine.n / 2);
  const float inv = 1.f / kernel_sum();
  parallel_rows(c.n,
                [&](int y)
                {
                  for (int x = 0; x < c.n; x++)
                  {
                    float sum = 0.f;
                    for (int j = 0; j < 5; j++)
                      for (int i = 0; i < 5; i++)
                        sum += KERNEL[j * 5 + i] *
                               edge(fine, 2 * x + i - 2, 2 * y + j - 2);
                    c.at(x, y) = sum * inv;
                  }
                });
  return c;
}

// parity-selected Gaussian taps, normalized by the taps actually used
int expand_taps(int x, int *c, int *k)
{
  if ((x & 1) == 0)
  {
    c[0] = x / 2 - 1, k[0] = 4;
    c[1] = x / 2, k[1] = 2;
    c[2] = x / 2 + 1, k[2] = 0;
    return 3;
  }
  c[0] = x >> 1, k[0] = 3;
  c[1] = (x >> 1) + 1, k[1] = 1;
  return 2;
}

float expand(const Field &coarse, int x, int y)
{
  int   cx[3], kx[3], cy[3], ky[3];
  int   nx = expand_taps(x, cx, kx), ny = expand_taps(y, cy, ky);
  float sum = 0.f, wsum = 0.f;
  for (int j = 0; j < ny; j++)
    for (int i = 0; i < nx; i++)
    {
      float w = KERNEL[ky[j] * 5 + kx[i]];
      sum += w * edge(coarse, cx[i], cy[j]);
      wsum += w;
    }
  return sum / wsum;
}

struct Level
{
  Field h;        // snow surface (terrain + snow), meters
  Field s;        // snow thickness
  Field a;        // snow after avalanche transfers
  Field residual; // fine detail lost by the next coarser level
  Field snowfall; // optional multipliers (empty if absent)
  Field melt;
  int   level = 0;
  float cell = 0.f;
};

struct Solver
{
  // configuration
  double scale = 0., talus_tan = 0., adhered = 0., intensity = 0., melting = 0.;
  double snow_line = 0., octaves = 0.;
  int    isnow = 0, idur = 0, passes = 1;

  // state carried across levels
  bool  first = true;
  float accum = 0.f;
  float f = 0.f;
  int   nsnow = 0;
  float snow_per_iter = 0.f, melt_per_iter = 0.f;
  int   iteration = 0;
  bool  final_pass = false;

  // avalanche transfers, written into a (rows in 4 interleaved phases)
  void step_a(Level &l) const
  {
    const int n = l.h.n;
    for (int phase = 0; phase < 4; phase++)
    {
      const int rows = (n - phase + 3) / 4;
      parallel_rows(rows, [&](int r) { step_a_row(l, phase + 4 * r); });
    }
  }

  void step_a_row(Level &l, int y) const
  {
    const int n = l.h.n;
    for (int x = 0; x < n; x++)
    {
      const float s = l.s.at(x, y);

      double d = 1.0 - (0.5 / adhered) * double(s);
      if (d <= 0.0)
        d = 0.0;
      float thr = float(d * double(float(double(l.cell) * talus_tan)));
      if (iteration == -1)
        thr = 0.f;

      if (!(0.f < s))
        continue;

      const float nw = edge(l.h, x - 1, y - 1), nn = edge(l.h, x, y - 1);
      const float ne = edge(l.h, x + 1, y - 1), ww = edge(l.h, x - 1, y);
      const float ee = edge(l.h, x + 1, y), sw = edge(l.h, x - 1, y + 1);
      const float ss = edge(l.h, x, y + 1), se = edge(l.h, x + 1, y + 1);
      const float c = l.h.at(x, y);

      float gx = (ne + ee + se + ee) - (nw + ww + sw + ww);
      float gy = (ss + se + sw + ss) - (ne + nn + nw + nn);
      float g = std::sqrt((gy * gy + gx * gx) * 0.015625f);

      float diff[8], total = 0.f, excess = 0.f;
      for (int k = 0; k < 8; k++)
      {
        int px = x + DX[k], py = y + DY[k];
        if (px < 0 || py < 0 || px >= n || py >= n || (!final_pass && g <= thr))
        {
          diff[k] = -1.f;
          continue;
        }
        float dh = c - l.h.at(px, py);
        diff[k] = dh;
        if (0.f < dh)
        {
          total += dh;
          float e = dh - thr * WN[k];
          if (e <= 0.f)
            e = 0.f;
          excess += e;
        }
      }

      float k_move = 0.125f;
      if (!final_pass)
      {
        k_move = 0.16f;
        excess = total;
      }

      float move = k_move * excess;
      if (s <= move)
        move = s;

      if (0.f < move)
      {
        for (int k = 0; k < 8; k++)
          if (0.f < diff[k])
            l.a.at(x + DX[k], y + DY[k]) += (diff[k] * move) / total;
        l.a.at(x, y) -= move;
      }
    }
  }

  // snowfall / melt and surface update
  void step_b(Level &l) const
  {
    const int  n = l.h.n;
    const bool use_sf = l.snowfall.n == n, use_melt = l.melt.n == n;

    parallel_rows(n,
                  [&](int y)
                  {
                    for (int x = 0; x < n; x++)
                    {
                      size_t i = size_t(y) * n + x;
                      float  sf = snow_per_iter, m = melt_per_iter;
                      float  a = l.a.data[i], h = l.h.data[i];

                      if (snow_line <= double(h))
                      {
                        if (use_sf)
                          sf *= l.snowfall.data[i];
                      }
                      else
                        sf = 0.f;

                      if (use_melt)
                        m *= l.melt.data[i];

                      if (first)
                      {
                        if (iteration < nsnow && iteration != -1)
                          a += sf;
                        a -= m;
                        if (a <= 0.f)
                          a = 0.f;
                      }

                      l.h.data[i] = (h - l.s.data[i]) + a;
                      l.s.data[i] = a;
                      l.a.data[i] = a;
                    }
                  });
  }

  void pass(Level &l)
  {
    step_a(l);
    step_b(l);
  }

  void simulate(Level &l)
  {
    // the init pass zeroes the transfer buffer, then the (always enabled)
    // thermal init copies the snow into it
    l.a = l.s;

    float melt = float(melting * 0.25);
    int   iters;

    if (first)
    {
      double d12 = scale * 0.25;
      f = float((double(idur) * d12) / double(float(idur) * l.cell));
      if (!(0.f <= f))
        f = 0.f;

      float f11 = float((double(idur - isnow) * d12 - double(accum)) / double(l.cell));
      if (!(0.f <= f11))
        f11 = 0.f;

      nsnow = int(float(isnow) * f);
      iters = int(float(nsnow) + f11);
      melt = iters > 0 ? (float(int(float(idur) * f)) / float(iters)) * melt : 0.f;
    }
    else
      iters = passes;

    float den = float(isnow) * f;
    snow_per_iter = (den > 0.f && f > 0.f)
                        ? float((double((float(nsnow) * 0.5f) / den) * intensity) /
                                double(f))
                        : 0.f;
    melt_per_iter = f > 0.f ? melt / f : 0.f;

    final_pass = false;
    for (iteration = 0; iteration < iters; iteration++)
      pass(l);
    iteration = std::max(iters, 0);

    final_pass = true;
    for (int k = 0; k < 6; k++)
      pass(l);

    if (l.level == 0)
    {
      iteration = -1;
      pass(l);
    }

    first = false;
  }

  void pyramid(Level &fine)
  {
    accum += (float(passes) + 0.5f) * fine.cell * 0.25f;

    Level coarse;
    coarse.level = fine.level + 1;
    coarse.cell = fine.cell + fine.cell;
    coarse.h = downsample(fine.h);
    coarse.s = Field(coarse.h.n);
    if (fine.snowfall.n == fine.h.n)
      coarse.snowfall = downsample(fine.snowfall);
    if (fine.melt.n == fine.h.n)
      coarse.melt = downsample(fine.melt);

    // detail of the fine level that the coarse level cannot represent
    fine.residual = Field(fine.h.n);
    parallel_rows(fine.h.n,
                  [&](int y)
                  {
                    for (int x = 0; x < fine.h.n; x++)
                      fine.residual.at(x, y) = fine.h.at(x, y) - expand(coarse.h, x, y);
                  });

    if (double(coarse.level) < octaves - 1.0 && coarse.h.n > 11)
      pyramid(coarse);

    simulate(coarse);

    // push the coarse result back: deep snow buries the fine detail
    parallel_rows(fine.h.n,
                  [&](int y)
                  {
                    for (int x = 0; x < fine.h.n; x++)
                    {
                      float hc = expand(coarse.h, x, y);
                      float sc = expand(coarse.s, x, y);
                      float r = fine.residual.at(x, y);
                      float rr = r;

                      if (0.f < sc)
                      {
                        if (0.f <= sc - r)
                        {
                          float k = 1.f - sc / coarse.cell;
                          if (k <= 0.f)
                            k = 0.f;
                          rr = k * r;
                          sc += (k - 1.f) * r;
                        }
                        else
                          rr = r - sc;
                      }

                      fine.h.at(x, y) = hc + rr;
                      fine.s.at(x, y) = sc <= 0.f ? 0.f : sc;
                    }
                  });
  }
};

Field resample(const Field &src, int n)
{
  if (src.n == n)
    return src;
  Field out(n);
  float k = float(src.n - 1) / float(std::max(1, n - 1));
  parallel_rows(n,
                [&](int y)
                {
                  for (int x = 0; x < n; x++)
                    out.at(x,
                           y) = bilinear(src.data, src.n, float(x) * k, float(y) * k, 0);
                });
  return out;
}

} // namespace

SnowResult snow(const Field      &height,
                const SnowParams &p,
                const Field      *p_snowfall,
                const Field      *p_melt)
{
  const int    n = height.n;
  const double size = std::max(p.terrain_width, 1.f);
  const double hr = std::max(p.terrain_height, 1e-3f);

  Solver s;
  s.scale = double(p.model_scale) * 4.0;
  s.talus_tan = std::tan(std::min(double(p.slip_off_angle), 89.999) * 0.0174532925199433);
  s.adhered = std::max(double(p.adhered_snow_mass), 1e-6);
  s.intensity = p.intensity;
  s.melting = p.melt;
  s.snow_line = double(p.snow_line) * hr;
  s.octaves = std::log(s.scale * 0.5 / (size / double(n))) * 1.4426950408889634;

  s.isnow = int(double(p.duration) * 1.1 * 256.0);
  int ipost = int(double(p.settle) * 1.1 * 256.0);
  s.idur = ipost + s.isnow;

  float fp = float(s.idur - s.isnow) * 0.25f, fd = float(s.idur) * 0.125f;
  if (fp <= fd)
    fp = fd;
  s.passes = std::max(1, int(fp));

  Level l0;
  l0.level = 0;
  l0.cell = float(size / double(n));
  l0.h = Field(n);
  for (size_t i = 0; i < l0.h.data.size(); i++)
    l0.h.data[i] = float(double(height.data[i]) * hr);
  l0.s = Field(n);
  if (p_snowfall && p_snowfall->n == n)
    l0.snowfall = *p_snowfall;
  if (p_melt && p_melt->n == n)
    l0.melt = *p_melt;

  if (1.0 < s.octaves)
    s.pyramid(l0);
  s.simulate(l0);

  SnowResult res;
  res.snow = Field(n);
  res.hard = Field(n);
  res.depth = Field(n);

  // soft coverage from the 3x3 neighborhood of snow deeper than 1 m
  parallel_rows(
      n,
      [&](int y)
      {
        for (int x = 0; x < n; x++)
        {
          float cnt = 0.f, sum = 0.f;
          for (int j = -1; j <= 1; j++)
            for (int i = -1; i <= 1; i++)
            {
              float w = (i == 0 && j == 0) ? 4.f : (i == 0 || j == 0) ? 2.f : 1.f;
              float v = edge(l0.s, x + i, y + j);
              if (1.f < v)
              {
                cnt += w;
                sum += (v - 1.f) * w;
              }
            }

          float sc = l0.s.at(x, y);
          float ec = sc - 1.f;
          if (ec <= 0.f)
            ec = 0.f;

          float cov = cnt - 4.f;
          if (cov <= 0.f)
            cov = 0.f;
          cov *= 0.108333334f;
          if (1.f <= cov)
            cov = 1.f;

          float dens = 0.f;
          if (0.f < ec)
          {
            dens = ((ec * 1.3f) / sum) * cnt;
            if (1.f <= dens)
              dens = 1.f;
          }

          float soft = dens * cov;
          if (0.f < soft)
            soft = soft * 0.8f + 0.2f;

          res.snow.at(x, y) = soft;
          res.hard.at(x, y) = sc <= 1.f ? 0.f : 1.f;
        }
      });

  // only snow above 1 m stays on the surface
  res.height = Field(n);
  for (size_t i = 0; i < l0.h.data.size(); i++)
  {
    float sv = l0.s.data[i];
    float ex = 1.f <= sv ? sv - 1.f : 0.f;
    float h = (ex - sv) + l0.h.data[i];
    res.depth.data[i] = float(double(ex) / hr);
    res.height.data[i] = float(double(h) / hr);
  }

  return res;
}

Field sun_exposure(const Field &height, const SunExposureParams &p)
{
  const int n = height.n;
  const int m = std::min(n, 512); // smooth field, computed at a capped size

  Field h = resample(height, m);
  for (float &v : h.data)
    v *= p.terrain_height;
  const float cell = p.terrain_width / float(m);

  // --- sun path samples (SunLightIntegral: stratified times over the day range)
  struct Sample
  {
    float x, y, z, w;
  };
  std::vector<Sample> samples;

  int   end = p.day_begin <= p.day_end ? p.day_end : p.day_end + 365;
  float days = float(end - p.day_begin + 1);
  int   count = int((days * 2000.f) / (days + 2.f));

  const float deg = 0.017453292f;
  const float lat = p.latitude * deg;
  uint32_t    seed = 0xa2351342u;
  float       jit = 0.5f;

  auto next = [&seed]()
  {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return float(seed * 0x2545f381u) * 2.3283064e-10f;
  };

  for (int i = 0; i < count; i++)
  {
    jit = (next() + jit) * 0.5f;
    float t = (days * (float(i) + jit)) / float(count) + float(p.day_begin);
    float day = float(int(t));
    float decl = 23.45f * std::sin((day + 284.f) * 0.98630136f * deg) * deg;
    float ha = ((t - day) * 24.f - 12.f) * 15.f;

    float elev = std::asin(std::sin(decl) * std::sin(lat) +
                           std::cos(decl) * std::cos(lat) * std::cos(ha * deg));
    float az = 180.f;
    if (ha != 0.f)
    {
      float c = (std::sin(decl) * std::cos(lat) -
                 std::cos(decl) * std::sin(lat) * std::cos(ha * deg)) /
                std::cos(elev);
      az = std::acos(std::clamp(c, -1.f, 1.f)) / deg;
      if (0.f < ha)
        az = 360.f - az;
    }

    float a = (az + p.north) * deg;
    float vx = std::sin(a), vy = -std::cos(a), vz = std::tan(elev);
    float len = 1.f / std::sqrt(vx * vx + vy * vy + vz * vz);
    float r = next();

    if (!(-0.15f < vz * len) || (vx == 0.f && vy == 0.f))
      continue;

    // atmospheric attenuation over the air mass at this elevation
    float se = std::sin(elev + (r * 0.5f - 0.25f) * deg);
    if (se < 0.f)
      se = 0.f;
    float air = 1.f / (se + 0.025f * std::exp(-11.f * se));
    samples.push_back({vx * len, vy * len, vz * len, std::exp(-0.17850628f * air)});
  }

  // --- horizon per azimuth bin, then cast-shadowed Lambert accumulation
  constexpr int                    BINS = 48;
  std::vector<std::vector<Sample>> bins(BINS);
  for (const Sample &s : samples)
  {
    float az = std::atan2(s.y, s.x);
    int   b = int(std::floor((az + PI) / (2.f * PI) * BINS)) % BINS;
    bins[size_t(std::max(b, 0))].push_back(s);
  }

  Field              energy(m);
  std::vector<float> horizon(size_t(m) * m);

  std::vector<float> dist;
  for (float d = 1.f; d < float(m); d = d < 8.f ? d + 1.f : d * 1.1f)
    dist.push_back(d);

  for (int b = 0; b < BINS; b++)
  {
    if (bins[b].empty())
      continue;
    float az = (float(b) + 0.5f) / BINS * 2.f * PI - PI;
    float dx = std::cos(az), dy = std::sin(az);

    parallel_rows(
        m,
        [&](int y)
        {
          for (int x = 0; x < m; x++)
          {
            float h0 = h.at(x, y), best = -1e30f;
            for (float d : dist)
            {
              float sx = float(x) + dx * d, sy = float(y) + dy * d;
              if (sx < 0.f || sy < 0.f || sx > float(m - 1) || sy > float(m - 1))
                break;
              best = std::max(best, (bilinear(h.data, m, sx, sy, 0) - h0) / (d * cell));
            }
            horizon[size_t(y) * m + x] = best; // tangent of the horizon angle

            // surface normal
            float gx = (edge(h, x + 1, y) - edge(h, x - 1, y)) / (2.f * cell);
            float gy = (edge(h, x, y + 1) - edge(h, x, y - 1)) / (2.f * cell);
            float inv = 1.f / std::sqrt(gx * gx + gy * gy + 1.f);
            float nx = -gx * inv, ny = -gy * inv, nz = inv;

            float e = 0.f;
            for (const Sample &s : bins[b])
            {
              float hz = std::sqrt(s.x * s.x + s.y * s.y);
              float tan_e = hz > 0.f ? s.z / hz : 1e30f;
              if (tan_e <= best)
                continue;
              float lam = nx * s.x + ny * s.y + nz * s.z;
              if (lam > 0.f)
                e += s.w * lam;
            }
            energy.at(x, y) += e;
          }
        });
  }

  normalize(energy);
  return resample(energy, n);
}

Field directional_melt_map(const Field &height,
                           float        direction,
                           float        remnants,
                           float        terrain_width,
                           float        terrain_height)
{
  SunExposureParams sp;
  sp.north = direction;
  sp.terrain_width = terrain_width;
  sp.terrain_height = terrain_height;
  Field     e = sun_exposure(height, sp);
  const int n = e.n;

  // histogram equalization, 16384 bins
  constexpr int         BINS = 16384;
  std::vector<uint32_t> hist(BINS, 0);
  auto bin = [](float v) { return std::clamp(int(v * float(BINS - 1)), 0, BINS - 1); };
  for (float v : e.data)
    hist[size_t(bin(v))]++;
  std::vector<float> cdf(BINS);
  double             acc = 0.;
  for (int i = 0; i < BINS; i++)
  {
    acc += hist[size_t(i)];
    cdf[size_t(i)] = float(acc / double(e.data.size()));
  }
  for (float &v : e.data)
    v = cdf[size_t(bin(v))];

  // light blur, radius 0.05 / 40 of the resolution (3 box passes)
  int r = int(std::lround(0.05f / 40.f * float(n)));
  for (int pass = 0; pass < 3 && r > 0; pass++)
  {
    Field t(n);
    parallel_rows(n,
                  [&](int y)
                  {
                    for (int x = 0; x < n; x++)
                    {
                      float s = 0.f;
                      for (int k = -r; k <= r; k++)
                        s += edge(e, x + k, y);
                      t.at(x, y) = s / float(2 * r + 1);
                    }
                  });
    parallel_rows(n,
                  [&](int y)
                  {
                    for (int x = 0; x < n; x++)
                    {
                      float s = 0.f;
                      for (int k = -r; k <= r; k++)
                        s += edge(t, x, y + k);
                      e.at(x, y) = s / float(2 * r + 1);
                    }
                  });
  }

  for (float &v : e.data)
    v = clamp01(v + remnants);
  return e;
}

} // namespace hesiod::landscape
