/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_PATH = "path";

constexpr const char *A_FNAME = "fname";

void setup_path_from_csv_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_PATH);

  // attribute(s)
  add_filename(node,
               A_FNAME,
               "fname",
               std::filesystem::path(""),
               "CSV files (*.csv)",
               false);
}

void compute_path_from_csv_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_out = node.get_value_ref<hmap::Path>(P_PATH);

  const std::string fname = node.val<std::filesystem::path>(A_FNAME).string();
  std::ifstream     f(fname.c_str());

  if (f.good())
  {
    hmap::Path path_tmp;
    bool       ret = path_tmp.from_csv(fname);
    if (ret)
      *p_out = path_tmp;
    else
      Logger::log()->error("compute_path_from_csv_node: could not parse CSV file {}",
                           fname);
  }
  else
  {
    Logger::log()->error("compute_path_from_csv_node: could not load CSV file {}", fname);
  }
}

} // namespace hesiod
