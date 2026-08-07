/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/cloud.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_OUT = "cloud";

constexpr const char *A_FNAME = "fname";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_cloud_from_csv_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::Cloud>(gnode::PortType::OUT, P_OUT);

  // --- Attributes

  node.set_current_category("Filename");
  add_filename(node,
               A_FNAME,
               "fname",
               std::filesystem::path(""),
               "CSV files (*.csv)",
               false);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_cloud_from_csv_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_out = node.get_value_ref<hmap::Cloud>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  const std::string fname = node.val<std::filesystem::path>(A_FNAME).string();

  // --- Compute

  std::ifstream f(fname.c_str());

  if (f.good())
  {
    hmap::Cloud cloud_tmp;
    bool        ret = cloud_tmp.from_csv(fname);
    if (ret)
      *p_out = cloud_tmp;
    else
      Logger::log()->error("compute_cloud_from_csv_node: could not parse CSV file {}",
                           fname);
  }
  else
  {
    Logger::log()->error("compute_cloud_from_csv_node: could not load CSV file {}",
                         fname);
  }
}

} // namespace hesiod
