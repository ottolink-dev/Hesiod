/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <fstream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

// in this order, required by args.hxx
#include "highmap/algebra.hpp"
static std::istream &operator>>(std::istream &is, glm::ivec2 &vec2)
{
  is >> vec2.x;
  is.get();
  is >> vec2.y;
  return is;
}
#pragma GCC diagnostic pop

#include <args.hxx>

#include "hesiod/model/graph/graph_config.hpp"

namespace hesiod::cli
{

// returns -1 to continue with the GUI (startup_file is filled if a project
// file was requested on the command line), or a process exit code (>= 0) if
// the invocation was fully handled here (batch modes, --help, parse errors)
int parse_args(args::ArgumentParser &parser,
               int                   argc,
               char                 *argv[],
               std::string          &startup_file);

void run_batch_mode(const std::string &filename,
                    const glm::ivec2  &shape,
                    const glm::ivec2  &tiling,
                    float              overlap,
                    const GraphConfig *p_input_model_config = nullptr);
void run_node_inventory();
void run_snapshot_generation();

// Verify every legacy .hsd under `dir` decodes correctly through the compat
// decoders, and that each decoded node round-trips through its `_meta`
// serialization. Returns a process exit code (0 = all good, 1 = failures).
int run_compat_check(const std::string &dir);

// True iff `decoded` (a parity-record value) matches the legacy per-key json.
bool values_equivalent(const nlohmann::json &decoded,
                       const nlohmann::json &legacy_attr,
                       const std::string    &type);

} // namespace hesiod::cli
