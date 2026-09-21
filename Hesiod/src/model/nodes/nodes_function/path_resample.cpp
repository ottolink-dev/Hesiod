/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

// Group: Interpolate
constexpr const char *G_INTERPOLATE       = "Interpolate";
constexpr const char *A_DELTA             = "delta";
constexpr const char *A_METHOD            = "method";
constexpr const char *A_CURVATURE_RATIO   = "curvature_ratio";
constexpr const char *A_CLOSED_PATH       = "closed_path";
constexpr const char *A_ENABLE_DECIMATE   = "enable_decimate";
constexpr const char *A_DECIMATE_SAMPLING = "decimate_sampling";

// Group: Decimate
constexpr const char *G_DECIMATE = "Decimate";
constexpr const char *A_NPOINTS  = "npoints";

// Group: Smooth
constexpr const char *G_SMOOTH              = "Smooth";
constexpr const char *A_NAVG                = "navg";
constexpr const char *A_AVERAGING_INTENSITY = "averaging_intensity";
constexpr const char *A_INERTIA             = "inertia";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_path_resample_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUT);

  // Group: Interpolate
  {
    node.set_current_group(G_INTERPOLATE);

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

    // clang-format off
    add_float(node, A_DELTA, "Step Size", 0.01f, 0.0001f, 0.1f, "{:.2e}", true);
    add_choice(node, A_METHOD, "Interpolation", methods, "Cubic");
    add_float(node, A_CURVATURE_RATIO, "Curvature Ratio", 0.3f, 0.f, 1.f);
    add_bool(node, A_CLOSED_PATH, "Close Path", false);
    add_bool(node, A_ENABLE_DECIMATE, "Enable Decimation", false);
    add_int(node, A_DECIMATE_SAMPLING, "Sampling Rate", 8, 2, INT_MAX);
    // clang-format on
  }

  // Group: Decimate
  {
    node.set_current_group(G_DECIMATE);

    add_int(node, A_NPOINTS, "Point Count Target", 8, 2, INT_MAX);
  }

  // Group: Smooth
  {
    node.set_current_group(G_SMOOTH);

    add_int(node, A_NAVG, "Averaging Window", 1, 1, 10);
    add_float(node, A_AVERAGING_INTENSITY, "Averaging Intensity", 1.f, 0.f, 1.f);
    add_float(node, A_INERTIA, "Inertia", 0.f, 0.f, 1.f);
  }

  // Reset active group to default
  node.set_current_group(G_INTERPOLATE);
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

  const std::string current_group = node.get_meta_group()
                                        .current_container_name()
                                        .value_or(G_INTERPOLATE);

  Logger::log()->trace("compute_path_resample_node: current_group {}", current_group);

  if (current_group == G_INTERPOLATE)
  {
    const auto delta             = node.val<float>(A_DELTA);
    const auto method            = node.val<std::string>(A_METHOD);
    const auto curvature_ratio   = node.val<float>(A_CURVATURE_RATIO);
    const auto closed_path       = node.val<bool>(A_CLOSED_PATH);
    const auto enable_decimate   = node.val<bool>(A_ENABLE_DECIMATE);
    const auto decimate_sampling = node.val<int>(A_DECIMATE_SAMPLING);

    const int  npoints = std::max(1, int(p_in->get_cumulative_distance().back() / delta));
    const auto edm     = hmap::Path::EdgeDivisionMode::EDM_FULL_ARC;

    *p_out = *p_in;

    if (enable_decimate)
      *p_out = hmap::decimate_vw(*p_out, decimate_sampling);

    p_out->set_closed(closed_path);

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
  else if (current_group == G_DECIMATE)
  {
    const auto npoints = node.val<int>(A_NPOINTS);
    *p_out             = hmap::decimate_vw(*p_in, npoints);
  }
  else if (current_group == G_SMOOTH)
  {
    const auto navg                = node.val<int>(A_NAVG);
    const auto averaging_intensity = node.val<float>(A_AVERAGING_INTENSITY);
    const auto inertia             = node.val<float>(A_INERTIA);

    *p_out = hmap::smooth(*p_in, navg, averaging_intensity, inertia);
  }
  else
  {
    Logger::log()->error("compute_path_resample_node: group {} not implemented",
                         current_group);
  }
}

} // namespace hesiod
