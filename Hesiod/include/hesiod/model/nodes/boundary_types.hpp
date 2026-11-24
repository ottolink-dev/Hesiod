/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "highmap/array.hpp"
#include "highmap/coord_frame.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/interpolate/interpolate_terrain.hpp"
#include "highmap/virtual_array/virtual_array.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"
#include "highmap/world/flora/forest.hpp"
#include "highmap/world/rocks/rock_field.hpp"

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
                                   const hmap::CoordFrame   *src_frame,
                                   hmap::VirtualArray       &dst,
                                   const GraphConfig        &dst_cfg,
                                   const hmap::CoordFrame   *dst_frame,
                                   hmap::VirtualArray       *p_mask = nullptr)
{
  if (!src || src->empty())
  {
    dst.fill(0.f, dst_cfg.cm_cpu);
    if (p_mask)
      p_mask->fill(0.f, dst_cfg.cm_cpu);
    return;
  }

  if (src_frame && dst_frame)
  {
    hmap::interpolate_terrain(*src,
                              dst,
                              *src_frame,
                              *dst_frame,
                              dst_cfg.cm_cpu,
                              true,
                              0.f,
                              p_mask);
    dst.sync_overlap_buffers();
    if (p_mask)
      p_mask->sync_overlap_buffers();
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
                                     const hmap::CoordFrame     *src_frame,
                                     hmap::VirtualTexture       &dst,
                                     const GraphConfig          &dst_cfg,
                                     const hmap::CoordFrame     *dst_frame,
                                     hmap::VirtualArray         *p_mask = nullptr)
{
  if (!src || src->channels() <= 0)
  {
    for (int c = 0; c < dst.channels(); ++c)
      dst.fill(c, c == 3 ? 1.f : 0.f, dst_cfg.cm_cpu);
    return;
  }

  const int nch = std::min(src->channels(), dst.channels());
  for (int c = 0; c < nch; ++c)
    transfer_virtual_array(&src->channel(c),
                           src_cfg,
                           src_frame,
                           dst.channel(c),
                           dst_cfg,
                           dst_frame,
                           c == 0 ? p_mask : nullptr);
}

template <typename CloudT>
inline void transfer_cloud_like(const CloudT           *src,
                                const hmap::CoordFrame *src_frame,
                                CloudT                 &dst,
                                const hmap::CoordFrame *dst_frame)
{
  if (!src)
  {
    dst = CloudT();
    return;
  }

  dst = *src;

  if (src_frame && dst_frame)
  {
    const hmap::AffineTransform2D tf = src_frame->compute_relative_transform(*dst_frame);
    for (auto &p : dst)
    {
      const glm::vec2 uv = tf.apply(p.x, p.y);
      p.x = uv.x;
      p.y = uv.y;
    }
  }
}

template <typename ScatterT>
inline void transfer_scatter_like(const ScatterT         *src,
                                  const hmap::CoordFrame *src_frame,
                                  ScatterT               &dst,
                                  const hmap::CoordFrame *dst_frame)
{
  if (!src)
  {
    dst = ScatterT();
    return;
  }

  dst = *src;

  if (src_frame && dst_frame)
  {
    const hmap::AffineTransform2D tf = src_frame->compute_relative_transform(*dst_frame);
    for (auto &item : dst)
    {
      const glm::vec2 uv = tf.apply(item.position.x, item.position.y);
      item.position.x = uv.x;
      item.position.y = uv.y;
    }
  }
}

template <typename T>
inline void transfer_boundary_value(const T * /*src*/,
                                    const GraphConfig & /*src_cfg*/,
                                    const hmap::CoordFrame * /*src_frame*/,
                                    T & /*dst*/,
                                    const GraphConfig & /*dst_cfg*/,
                                    const hmap::CoordFrame * /*dst_frame*/,
                                    hmap::VirtualArray * /*p_mask*/ = nullptr)
{
  // intentionally empty for non-VA/VT types
}

template <>
inline void transfer_boundary_value<hmap::VirtualArray>(const hmap::VirtualArray *src,
                                                        const GraphConfig        &src_cfg,
                                                        const hmap::CoordFrame *src_frame,
                                                        hmap::VirtualArray     &dst,
                                                        const GraphConfig      &dst_cfg,
                                                        const hmap::CoordFrame *dst_frame,
                                                        hmap::VirtualArray     *p_mask)
{
  transfer_virtual_array(src, src_cfg, src_frame, dst, dst_cfg, dst_frame, p_mask);
}

template <>
inline void transfer_boundary_value<hmap::VirtualTexture>(
    const hmap::VirtualTexture *src,
    const GraphConfig          &src_cfg,
    const hmap::CoordFrame     *src_frame,
    hmap::VirtualTexture       &dst,
    const GraphConfig          &dst_cfg,
    const hmap::CoordFrame     *dst_frame,
    hmap::VirtualArray         *p_mask)
{
  transfer_virtual_texture(src, src_cfg, src_frame, dst, dst_cfg, dst_frame, p_mask);
}

template <>
inline void transfer_boundary_value<hmap::Array>(const hmap::Array *src,
                                                 const GraphConfig & /*src_cfg*/,
                                                 const hmap::CoordFrame * /*src_frame*/,
                                                 hmap::Array &dst,
                                                 const GraphConfig & /*dst_cfg*/,
                                                 const hmap::CoordFrame * /*dst_frame*/,
                                                 hmap::VirtualArray * /*p_mask*/)
{
  dst = src ? *src : hmap::Array();
}

template <>
inline void transfer_boundary_value<hmap::Cloud>(const hmap::Cloud *src,
                                                 const GraphConfig & /*src_cfg*/,
                                                 const hmap::CoordFrame *src_frame,
                                                 hmap::Cloud            &dst,
                                                 const GraphConfig & /*dst_cfg*/,
                                                 const hmap::CoordFrame *dst_frame,
                                                 hmap::VirtualArray * /*p_mask*/)
{
  transfer_cloud_like(src, src_frame, dst, dst_frame);
}

template <>
inline void transfer_boundary_value<hmap::Path>(const hmap::Path *src,
                                                const GraphConfig & /*src_cfg*/,
                                                const hmap::CoordFrame *src_frame,
                                                hmap::Path             &dst,
                                                const GraphConfig & /*dst_cfg*/,
                                                const hmap::CoordFrame *dst_frame,
                                                hmap::VirtualArray * /*p_mask*/)
{
  transfer_cloud_like(src, src_frame, dst, dst_frame);
}

template <>
inline void transfer_boundary_value<hmap::Forest>(const hmap::Forest *src,
                                                  const GraphConfig & /*src_cfg*/,
                                                  const hmap::CoordFrame *src_frame,
                                                  hmap::Forest           &dst,
                                                  const GraphConfig & /*dst_cfg*/,
                                                  const hmap::CoordFrame *dst_frame,
                                                  hmap::VirtualArray * /*p_mask*/)
{
  transfer_scatter_like(src, src_frame, dst, dst_frame);
}

template <>
inline void transfer_boundary_value<hmap::RockField>(const hmap::RockField *src,
                                                     const GraphConfig & /*src_cfg*/,
                                                     const hmap::CoordFrame *src_frame,
                                                     hmap::RockField        &dst,
                                                     const GraphConfig & /*dst_cfg*/,
                                                     const hmap::CoordFrame *dst_frame,
                                                     hmap::VirtualArray * /*p_mask*/)
{
  transfer_scatter_like(src, src_frame, dst, dst_frame);
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
