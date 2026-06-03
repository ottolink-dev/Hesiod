/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <sstream>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_config.hpp"

namespace hesiod
{

GraphConfig::GraphConfig()
{
  Logger::log()->trace("GraphConfig::GraphConfig");

  AppContext &ctx = HSD_CTX;

  this->shape = {ctx.app_settings.node_editor.default_resolution,
                 ctx.app_settings.node_editor.default_resolution};
  this->tiling = {ctx.app_settings.node_editor.default_tiling,
                  ctx.app_settings.node_editor.default_tiling};
  this->overlap = ctx.app_settings.node_editor.default_overlap;

  this->update_parameters();
}

std::string GraphConfig::info_string() const
{
  std::ostringstream oss;

  oss << "GraphConfig\n";
  oss << "  Shape          : " << shape.x << " x " << shape.y << "\n";
  oss << "  Tiling         : " << tiling.x << " x " << tiling.y << "\n";
  oss << "  Overlap        : " << overlap << "\n";
  oss << "  Tile shape     : " << tile_shape.x << " x " << tile_shape.y << "\n";
  oss << "  Halo           : " << halo << "\n";
  oss << "\n";
  oss << "  Storage mode   : " << hmap::to_string(storage_mode) << "\n";
  oss << "\n";
  oss << "  Compute modes\n";
  oss << "    CPU           : " << hmap::to_string(cm_cpu.mode)
      << " (trim=" << cm_cpu.trim_storage << ")\n";
  oss << "    GPU           : " << hmap::to_string(cm_gpu.mode)
      << " (trim=" << cm_gpu.trim_storage << ")\n";
  oss << "    Single array  : " << hmap::to_string(cm_single_array.mode)
      << " (trim=" << cm_single_array.trim_storage << ")\n";

  return oss.str();
}

void GraphConfig::log_debug() const { Logger::log()->debug("\n{}", this->info_string()); }

void GraphConfig::json_from(nlohmann::json const &json)
{
  this->shape.x = json["shape.x"];
  this->shape.y = json["shape.y"];
  this->tiling.x = json["tiling.x"];
  this->tiling.y = json["tiling.y"];
  this->overlap = json["overlap"];

  this->update_parameters();
}

nlohmann::json GraphConfig::json_to() const
{
  nlohmann::json json;
  json["shape.x"] = this->shape.x;
  json["shape.y"] = this->shape.y;
  json["tiling.x"] = this->tiling.x;
  json["tiling.y"] = this->tiling.y;
  json["overlap"] = this->overlap;
  return json;
}

void GraphConfig::set_shape(const glm::ivec2 &new_shape)
{
  this->shape = new_shape;
  this->update_parameters();
}

void GraphConfig::set_tiling(const glm::ivec2 &new_tiling)
{
  this->tiling = new_tiling;
  this->update_parameters();
}

void GraphConfig::set_overlap(float new_overlap)
{
  this->overlap = new_overlap;
  this->update_parameters();
}

void GraphConfig::update_parameters()
{
  int w = this->shape.x / this->tiling.x;
  int h = this->shape.y / this->tiling.y;

  this->tile_shape = {w, h};
  this->halo = std::max(int(this->overlap * w), int(this->overlap * h));

  this->log_debug();
}

} // namespace hesiod
