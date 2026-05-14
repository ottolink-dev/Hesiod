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

int parse_args(args::ArgumentParser &parser, int argc, char *argv[]);

void run_batch_mode(const std::string &filename,
                    const glm::ivec2  &shape,
                    const glm::ivec2  &tiling,
                    float              overlap,
                    const GraphConfig *p_input_model_config = nullptr);
void run_node_inventory();
void run_snapshot_generation();

} // namespace hesiod::cli
