/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <cassert>
#include <iostream>
#include <memory>

#include <QTemporaryDir>
#include <QtTest>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/model/graph/graph_config.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/boundary_nodes.hpp"
#include "hesiod/model/nodes/subgraph_node.hpp"

// A dummy source node that provides an output VirtualArray
class SourceNode : public hesiod::BaseNode
{
public:
  SourceNode(std::weak_ptr<hesiod::GraphConfig> config) : BaseNode("SourceNode", config)
  {
    this->add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG((*this)));
  }

  void compute() override {}
};

int main(int argc, char **argv)
{
  QTemporaryDir config_dir;
  qputenv("XDG_CONFIG_HOME", config_dir.path().toUtf8());
  qputenv("XDG_CACHE_HOME", config_dir.path().toUtf8());

  hesiod::HesiodApplication app(argc, argv, hesiod::HesiodApplication::StartupMode::ContextOnly);

  std::cout << "Running test_subgraph_system..." << std::endl;

  auto cfg_parent = std::make_shared<hesiod::GraphConfig>();
  cfg_parent->set_shape(glm::ivec2(64, 64));
  cfg_parent->set_tiling(glm::ivec2(2, 2));
  cfg_parent->storage_mode = hmap::StorageMode::VA_RAM;

  auto cfg_child = std::make_shared<hesiod::GraphConfig>();
  cfg_child->set_shape(glm::ivec2(64, 64));
  cfg_child->set_tiling(glm::ivec2(2, 2));
  cfg_child->storage_mode = hmap::StorageMode::VA_RAM;

  // 1. Create inner GraphNode
  auto inner_graph = std::make_shared<hesiod::GraphNode>("inner_graph", cfg_child);

  auto in_node = std::make_shared<hesiod::NodeIn<hmap::VirtualArray>>("elevation", cfg_child);
  auto out_node = std::make_shared<hesiod::NodeOut<hmap::VirtualArray>>("result", cfg_child);

  inner_graph->add_node(in_node, "in_1");
  inner_graph->add_node(out_node, "out_1");
  inner_graph->new_link("in_1", "value", "out_1", "value");

  // 2. Encapsulate in SubGraphNode
  auto sub_node = std::make_shared<hesiod::SubGraphNode>("SubTerrain", cfg_parent, inner_graph);
  sub_node->auto_bind_ports();

  assert(sub_node->has_port("elevation"));
  assert(sub_node->has_port("result"));

  // 3. Put in parent graph and link from a source node
  auto parent_graph = std::make_shared<hesiod::GraphNode>("parent_graph", cfg_parent);
  auto src_node = std::make_shared<SourceNode>(cfg_parent);

  parent_graph->add_node(src_node, "src_1");
  parent_graph->add_node(sub_node, "sub_1");
  parent_graph->new_link("src_1", "output", "sub_1", "elevation");

  // Populate source node's output
  hmap::VirtualArray *p_src = src_node->get_value_ref<hmap::VirtualArray>("output");
  assert(p_src != nullptr);
  p_src->fill(42.0f, cfg_parent->cm_cpu);

  // 4. Update parent graph
  parent_graph->update();

  hmap::VirtualArray *p_out = sub_node->get_value_ref<hmap::VirtualArray>("result");
  assert(p_out != nullptr);
  float out_min = p_out->min(cfg_parent->cm_cpu);
  float out_max = p_out->max(cfg_parent->cm_cpu);

  std::cout << "SubGraph output range: [" << out_min << ", " << out_max << "]" << std::endl;
  assert(out_min == 42.0f && out_max == 42.0f);

  std::cout << "All SubGraph tests passed successfully!" << std::endl;
  return 0;
}
