/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "path";

constexpr const char *A_DELTA             = "delta";
constexpr const char *A_METHOD            = "method";
constexpr const char *A_CLOSED_PATH       = "closed_path";
constexpr const char *A_ENABLE_DECIMATE   = "enable_decimate";
constexpr const char *A_DECIMATE_SAMPLING = "decimate_sampling";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

void setup_path_resample_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUT);

  std::vector<std::string> methods = {"Bezier",
                                      "Bezier Round",
                                      "BSpline",
                                      "Catmullrom",
                                      "Decasteljau",
                                      "Cubic",
                                      "Cubic Per.",
                                      "Akima",
                                      "Akima Per.",
                                      "Linear"};

  // attribute(s)
  // clang-format off
  add_float(node, A_DELTA, "Step Size", 0.01f, 0.0001f, 0.1f, "{:.2e}", true);
  add_choice(node, A_METHOD, "Interpolation", methods, "Cubic");  
  add_bool(node, A_CLOSED_PATH, "Close Path", false);
  add_bool(node, A_ENABLE_DECIMATE, "Enable Decimation", false);
  add_int(node, A_DECIMATE_SAMPLING, "Sampling Rate", 8, 2, INT_MAX);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_path_resample_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in  = node.get_value_ref<hmap::Path>(P_IN);
  hmap::Path *p_out = node.get_value_ref<hmap::Path>(P_OUT);

  if (!p_in || p_in->size() < 2)
    return;

  // --- Params

  // clang-format off
  const auto  delta             = node.val<float>(A_DELTA);
  const auto  method            = node.val<std::string>(A_METHOD);
  const auto  closed_path       = node.val<bool>(A_CLOSED_PATH);
  const auto  enable_decimate   = node.val<bool>(A_ENABLE_DECIMATE);
  const auto  decimate_sampling = node.val<int>(A_DECIMATE_SAMPLING);
  //
  const int   npoints           = std::max(1, int(p_in->get_cumulative_distance().back() / delta));
  const float curvature_ratio   = 0.3f;
  const auto  edm               = hmap::Path::EdgeDivisionMode::EDM_FULL_ARC;
  // clang-format on

  // --- Compute

  *p_out = *p_in;

  // prepro
  if (enable_decimate)
    *p_out = hmap::decimate_vw(*p_out, decimate_sampling);

  p_out->set_closed(closed_path);

  // interpolate
  using IM1D = hmap::InterpolationMethod1D;

  if (method == "Bezier")
    *p_out = hmap::bezier(*p_out, curvature_ratio, npoints, edm);
  else if (method == "Bezier Round")
    *p_out = hmap::bezier_round(*p_out, curvature_ratio, npoints, edm);
  else if (method == "BSpline")
    *p_out = hmap::bspline(*p_out, npoints, edm);
  else if (method == "Catmullrom")
    *p_out = hmap::catmullrom(*p_out, npoints, edm);
  else if (method == "Decasteljau")
    *p_out = hmap::decasteljau(*p_out, npoints, edm);
  else if (method == "Cubic")
    p_out->resample_interp(npoints, IM1D::CUBIC);
  else if (method == "Cubic Per.")
    p_out->resample_interp(npoints, IM1D::CUBIC_PERIODIC);
  else if (method == "Akima")
    p_out->resample_interp(npoints, IM1D::AKIMA);
  else if (method == "Akima Per.")
    p_out->resample_interp(npoints, IM1D::AKIMA_PERIODIC);
  else if (method == "Linear")
    p_out->resample_interp(npoints, IM1D::LINEAR);
  else
    Logger::log()->error("compute_path_resample_node: unknown interpolation method: {}",
                         method);
}

} // namespace hesiod
