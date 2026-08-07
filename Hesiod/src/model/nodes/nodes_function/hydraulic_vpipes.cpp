/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_BEDROCK     = "bedrock";
constexpr const char *P_IN          = "input";
constexpr const char *P_MASK        = "mask";
constexpr const char *P_MOISTURE    = "moisture";
constexpr const char *P_OUT         = "output";
constexpr const char *P_SEDIMENT    = "sediment";
constexpr const char *P_WATER_DEPTH = "water_depth";

constexpr const char *A_DOWNCUTTING_MAX_DEPTH_RATIO = "downcutting_max_depth_ratio";
constexpr const char *A_DURATION                    = "duration";
constexpr const char *A_EVAP_RATE                   = "evap_rate";
constexpr const char *A_FLUX_DIFFUSION              = "flux_diffusion";
constexpr const char *A_FLUX_DIFFUSION_STRENGTH     = "flux_diffusion_strength";
constexpr const char *A_ITERATIONS                  = "iterations";
constexpr const char *A_K_CAPACITY                  = "k_capacity";
constexpr const char *A_K_DEPOSE                    = "k_depose";
constexpr const char *A_K_DISCHARGE_EXP             = "k_discharge_exp";
constexpr const char *A_K_ERODE                     = "k_erode";
constexpr const char *A_MAINTAIN_WATER_VOLUME       = "maintain_water_volume";
constexpr const char *A_WATER_HEIGHT                = "water_height";

void setup_hydraulic_vpipes_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BEDROCK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MOISTURE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SEDIMENT, CONFIG(node));

  // attribute(s)
  // add_float(node, A_DURATION, "Duration", 0.01f, 0.01f, 0.5f);
  add_int(node, A_ITERATIONS, "iterations", 30, 1, 200);
  add_float(node,
            A_WATER_HEIGHT,
            "water_height",
            0.01f,
            0.001f,
            0.1f,
            "{:.2e}",
            /* log */ true);
  add_bool(node, A_MAINTAIN_WATER_VOLUME, "maintain_water_volume", true);
  add_float(node, A_EVAP_RATE, "evap_rate", 0.01f, 0.f, 0.5f);

  add_float(node, A_K_CAPACITY, "k_capacity", 0.5f, 0.01f, 2.f);
  add_float(node, A_K_ERODE, "k_erode", 0.002f, 0.f, 0.1f);
  add_float(node, A_K_DEPOSE, "k_depose", 0.01f, 0.f, 0.1f);
  add_float(node, A_K_DISCHARGE_EXP, "k_discharge_exp", 1.f, 0.1f, 2.f);

  add_float(node,
            A_DOWNCUTTING_MAX_DEPTH_RATIO,
            "downcutting_max_depth_ratio",
            1.5f,
            0.01f,
            3.f);

  add_bool(node, A_FLUX_DIFFUSION, "flux_diffusion", true);
  add_float(node,
            A_FLUX_DIFFUSION_STRENGTH,
            "flux_diffusion_strength",
            0.01f,
            1e-4f,
            1e-1f,
            "{:.2e}",
            true);
}

void compute_hydraulic_vpipes_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_bedrock = node.get_value_ref<hmap::VirtualArray>(P_BEDROCK);
    hmap::VirtualArray *p_moisture_map = node.get_value_ref<hmap::VirtualArray>(
        P_MOISTURE);
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);

    hmap::VirtualArray *p_out         = node.get_value_ref<hmap::VirtualArray>(P_OUT);
    hmap::VirtualArray *p_water_depth = node.get_value_ref<hmap::VirtualArray>(
        P_WATER_DEPTH);
    hmap::VirtualArray *p_sediment = node.get_value_ref<hmap::VirtualArray>(P_SEDIMENT);

    // int iterations = int(node.val<float>(A_DURATION) *
    // p_out->shape.x);
    int iterations = node.val<int>(A_ITERATIONS);

    hmap::for_each_tile(
        {p_in, p_bedrock, p_moisture_map, p_mask},
        {p_out, p_water_depth, p_sediment},
        [&node, iterations](std::vector<const hmap::Array *> p_arrays_in,
                            std::vector<hmap::Array *>       p_arrays_out,
                            const hmap::TileRegion &)
        {
          const auto [pa_in, pa_bedrock, pa_moisture_map, pa_mask] = unpack<4>(
              p_arrays_in);
          auto [pa_out, pa_water_depth, pa_sediment] = unpack<3>(p_arrays_out);

          *pa_out = *pa_in;

          hmap::gpu::hydraulic_vpipes(*pa_out,
                                      node.val<float>(A_WATER_HEIGHT),
                                      node.val<bool>(A_MAINTAIN_WATER_VOLUME),
                                      node.val<float>(A_EVAP_RATE),
                                      iterations,
                                      /* dt */ 0.5f,
                                      node.val<float>(A_K_CAPACITY),
                                      node.val<float>(A_K_ERODE),
                                      node.val<float>(A_K_DEPOSE),
                                      node.val<float>(A_K_DISCHARGE_EXP),
                                      node.val<float>(A_DOWNCUTTING_MAX_DEPTH_RATIO),
                                      node.val<bool>(A_FLUX_DIFFUSION),
                                      node.val<float>(A_FLUX_DIFFUSION_STRENGTH)
                                      //  *p_rain_map = nullptr,
                                      // Array *p_water_depth = nullptr,
                                      // Array *p_sediment = nullptr,
                                      // Array *p_vel_u = nullptr,
                                      // Array *p_vel_v = nullptr
          );
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // if (p_erosion_map)
    // {
    //   p_erosion_map->smooth_overlap_buffers();
    //   p_erosion_map->remap(0.f, 1.f, node.cfg().cm_cpu);
    // }

    // if (p_deposition_map)
    // {
    //   p_deposition_map->smooth_overlap_buffers();
    //   p_deposition_map->remap(0.f, 1.f, node.cfg().cm_cpu);
    // }
  }
}

} // namespace hesiod
