/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "highmap/array.hpp"
#include "highmap/flora/forest.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/rocks/rock_field.hpp"
#include "highmap/virtual_array/virtual_array.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/graph/graph_config.hpp"

namespace hesiod
{

inline const std::vector<std::string> &boundary_data_types()
{
  static const std::vector<std::string> types = {
      "VirtualArray",
      "VirtualTexture",
      "Array",
      "Cloud",
      "Path",
      "Forest",
      "RockField",
  };
  return types;
}

inline void transfer_virtual_array(const hmap::VirtualArray *src,
                                   const GraphConfig        &src_cfg,
                                   hmap::VirtualArray       &dst,
                                   const GraphConfig        &dst_cfg)
{
  if (!src || src->empty())
  {
    dst.fill(0.f, dst_cfg.cm_cpu);
    return;
  }

  if (src->shape == dst.shape && src->tile_shape == dst.tile_shape &&
      src->halo == dst.halo)
  {
    hmap::copy_data(*src, dst, dst_cfg.cm_cpu);
  }
  else
  {
    hmap::Array arr = src->to_array(src_cfg.cm_cpu);
    dst.from_array_bilinear(arr, dst_cfg.cm_cpu);
    dst.sync_overlap_buffers();
  }
}

inline void transfer_virtual_texture(const hmap::VirtualTexture *src,
                                     const GraphConfig          &src_cfg,
                                     hmap::VirtualTexture       &dst,
                                     const GraphConfig          &dst_cfg)
{
  if (!src || src->channels() <= 0)
  {
    for (int c = 0; c < dst.channels(); ++c)
      dst.fill(c, c == 3 ? 1.f : 0.f, dst_cfg.cm_cpu);
    return;
  }

  const int nch = std::min(src->channels(), dst.channels());
  for (int c = 0; c < nch; ++c)
    transfer_virtual_array(&src->channel(c), src_cfg, dst.channel(c), dst_cfg);
}

template <typename T>
inline void transfer_boundary_value(const T *src,
                                    const GraphConfig & /*src_cfg*/,
                                    T &dst,
                                    const GraphConfig & /*dst_cfg*/)
{
  dst = src ? *src : T();
}

template <>
inline void transfer_boundary_value<hmap::VirtualArray>(const hmap::VirtualArray *src,
                                                        const GraphConfig        &src_cfg,
                                                        hmap::VirtualArray       &dst,
                                                        const GraphConfig        &dst_cfg)
{
  transfer_virtual_array(src, src_cfg, dst, dst_cfg);
}

template <>
inline void transfer_boundary_value<hmap::VirtualTexture>(const hmap::VirtualTexture *src,
                                                          const GraphConfig    &src_cfg,
                                                          hmap::VirtualTexture &dst,
                                                          const GraphConfig    &dst_cfg)
{
  transfer_virtual_texture(src, src_cfg, dst, dst_cfg);
}

template <>
inline void transfer_boundary_value<hmap::Array>(const hmap::Array *src,
                                                 const GraphConfig & /*src_cfg*/,
                                                 hmap::Array       &dst,
                                                 const GraphConfig &dst_cfg)
{
  if (!src || src->size() == 0)
    dst = hmap::Array(dst_cfg.shape);
  else if (src->shape == dst_cfg.shape)
    dst = *src;
  else
    dst = src->resample_to_shape_bilinear(dst_cfg.shape);
}

template <typename Fn> inline void visit_boundary_type(const std::string &dtype, Fn &&fn)
{
  if (dtype == "VirtualArray")
    fn.template operator()<hmap::VirtualArray>();
  else if (dtype == "VirtualTexture")
    fn.template operator()<hmap::VirtualTexture>();
  else if (dtype == "Array")
    fn.template operator()<hmap::Array>();
  else if (dtype == "Cloud")
    fn.template operator()<hmap::Cloud>();
  else if (dtype == "Path")
    fn.template operator()<hmap::Path>();
  else if (dtype == "Forest")
    fn.template operator()<hmap::Forest>();
  else if (dtype == "RockField")
    fn.template operator()<hmap::RockField>();
  else
    fn.template operator()<hmap::VirtualArray>();
}

} // namespace hesiod
