/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/project_model.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

ProjectModel::ProjectModel()
{
  Logger::log()->trace("ProjectModel::ProjectModel");
  this->initialize();
}

void ProjectModel::cleanup()
{
  Logger::log()->trace("ProjectModel::cleanup");

  this->graph_manager.reset();
  this->graph_manager = std::make_unique<GraphManager>();

  this->path = std::filesystem::path();
  this->name = std::string();
  this->bake_config = BakeConfig();
  this->is_dirty = false;
  this->load_errors.clear();
}

void ProjectModel::add_load_error(const std::string &error)
{
  this->load_errors.push_back(error);
}

void ProjectModel::clear_load_errors() { this->load_errors.clear(); }

const std::vector<std::string> &ProjectModel::get_load_errors() const
{
  return this->load_errors;
}

BakeConfig ProjectModel::get_bake_config() const { return this->bake_config; }

std::string ProjectModel::get_comment() const { return this->comment; }

GraphManager *ProjectModel::get_graph_manager_ref() { return this->graph_manager.get(); }

bool ProjectModel::get_is_dirty() const { return this->is_dirty; }

std::string ProjectModel::get_name() const { return this->name; }

std::filesystem::path ProjectModel::get_path() const { return this->path; }

void ProjectModel::initialize()
{
  Logger::log()->trace("ProjectModel::initialize");
  this->graph_manager = std::make_unique<GraphManager>();
  this->load_errors.clear();
}

void ProjectModel::json_from(nlohmann::json const &json)
{
  Logger::log()->trace("ProjectModel::json_from");

  this->load_errors.clear();

  json_safe_get(json, "comment", this->comment);

  // bake
  if (json.contains("bake_config"))
    this->bake_config.json_from(json["bake_config"]);
  else
    Logger::log()->error("ProjectModel::json_from: could not parse bake_config json");

  // graphs
  if (json.contains("graph_manager"))
  {
    try
    {
      this->graph_manager->json_from(json["graph_manager"]);
      this->graph_manager->update();
      for (const auto &err : this->graph_manager->get_load_errors())
        this->add_load_error(err);
    }
    catch (const std::exception &e)
    {
      const std::string err = std::format("Failed to load graph manager: {}", e.what());
      Logger::log()->error("{}", err);
      this->add_load_error(err);
    }
    catch (...)
    {
      const std::string err = "Failed to load graph manager: unknown error";
      Logger::log()->error("{}", err);
      this->add_load_error(err);
    }
  }
  else
  {
    const std::string err = "ProjectModel::json_from: could not parse graph_manager json";
    Logger::log()->error("{}", err);
    this->add_load_error(err);
  }
}

nlohmann::json ProjectModel::json_to() const
{
  Logger::log()->trace("ProjectModel::json_to");

  nlohmann::json json;
  json["comment"] = this->comment;
  json["bake_config"] = this->bake_config.json_to();
  json["graph_manager"] = this->graph_manager->json_to();
  return json;
}

void ProjectModel::on_has_changed()
{
  if (this->has_changed)
    this->has_changed();

  this->set_is_dirty(true);
}

void ProjectModel::set_bake_config(const BakeConfig &new_bake_config)
{
  this->bake_config = new_bake_config;
}

void ProjectModel::set_comment(const std::string &new_comment)
{
  this->comment = new_comment;
}

void ProjectModel::set_is_dirty(bool new_state)
{
  if (new_state != this->is_dirty)
  {
    this->is_dirty = new_state;

    if (this->is_dirty_changed)
      this->is_dirty_changed();
  }
}

void ProjectModel::set_path(const std::filesystem::path &new_path)
{
  this->path = new_path;
  this->name = this->path.stem().string();

  if (this->project_name_changed)
    this->project_name_changed();
}

void ProjectModel::set_path(const std::string &new_path)
{
  this->set_path(std::filesystem::path(new_path));
}

void ProjectModel::set_name(const std::string &new_name)
{
  this->name = new_name;
  this->path = this->path.parent_path() / (this->name + ".hsd");

  if (this->project_name_changed)
    this->project_name_changed();
}

} // namespace hesiod
