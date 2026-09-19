/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <set>
#include <tuple>

#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include "gnodegui/style.hpp"
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/graph_editor.hpp"
#include "hesiod/gui/hesiod_node_proxy.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/base_node.hpp"

using namespace hesiod;

namespace
{
std::shared_ptr<GraphConfig> small_config()
{
  auto config = std::make_shared<GraphConfig>();
  config->set_shape({8, 8});
  config->set_tiling({1, 1});
  config->set_overlap(0.f);
  config->storage_mode = hmap::StorageMode::VA_RAM;
  return config;
}

bool consistent(GraphNode &graph, gngui::GraphViewer &view)
{
  std::set<std::string> model_ids, view_ids;
  using Edge = std::tuple<std::string, std::string, std::string, std::string>;
  std::set<Edge> model_links, view_links;
  for (const auto &[id, node] : graph.get_nodes())
    model_ids.insert(id);
  const auto json = view.json_to();
  for (const auto &node : json.at("nodes"))
    view_ids.insert(node.at("id").get<std::string>());
  for (const auto &link : graph.get_links())
    model_links.emplace(link.from,
                        graph.get_node(link.from)->get_port_label(link.port_from),
                        link.to,
                        graph.get_node(link.to)->get_port_label(link.port_to));
  for (const auto &link : json.at("links"))
    view_links.emplace(link.at("node_out_id").get<std::string>(),
                       link.at("port_out_id").get<std::string>(),
                       link.at("node_in_id").get<std::string>(),
                       link.at("port_in_id").get<std::string>());
  return model_ids == view_ids && model_links == view_links &&
         view_ids.size() == json.at("nodes").size() &&
         view_links.size() == json.at("links").size();
}

class UnavailableOutputProxy : public HesiodNodeProxy
{
public:
  using HesiodNodeProxy::HesiodNodeProxy;
  std::string get_port_id(int index) const override
  {
    return index == 1 ? "unavailable" : HesiodNodeProxy::get_port_id(index);
  }
};

struct Fixture
{
  std::shared_ptr<GraphConfig> config = small_config();
  std::shared_ptr<GraphNode>   graph = std::make_shared<GraphNode>("test", config);
  gngui::GraphViewer           view;
  int                          updates = 0;
  int                          edits = 0;
  std::map<std::string, int>   computed;
  std::vector<std::string>     created, deleted;
  bool                         notifications_consistent = true;
  bool                         fail_presentation = false;
  bool                         fail_reconnection = false;
  GraphEditor                  editor;

  Fixture() : editor(graph, view, callbacks())
  {
    view.scene()->setParent(&view);
    graph->update_started = [this]() { ++updates; };
    graph->compute_finished = [this](const std::string &id) { ++computed[id]; };
  }

  GraphEditor::NodePresentation callbacks()
  {
    GraphEditor::NodePresentation result;
    result.create = [this](const std::string &id, QPointF position)
    {
      auto              node = graph->get_node_ref_by_id<BaseNode>(id)->get_shared();
      gngui::NodeProxy *proxy = fail_reconnection
                                    ? static_cast<gngui::NodeProxy *>(
                                          new UnavailableOutputProxy(node, &view))
                                    : new HesiodNodeProxy(node, &view);
      view.add_node(proxy, position, id);
      if (fail_presentation)
        throw std::runtime_error("Injected presentation failure");
    };
    result.created = [this](const std::string &id)
    {
      created.push_back(id);
      notifications_consistent &= consistent(*graph, view);
    };
    result.deleted = [this](const std::string &id)
    {
      deleted.push_back(id);
      notifications_consistent &= consistent(*graph, view);
    };
    result.changed = [this]() { ++edits; };
    return result;
  }

  ~Fixture()
  {
    // Destroy links before their node proxies/model references become invalid.
    for (const auto &[id, node] : graph->get_nodes())
      view.erase_node(id);
  }

  void reset_counts()
  {
    updates = 0;
    edits = 0;
    computed.clear();
    created.clear();
    deleted.clear();
  }

  std::string add(const std::string &type = "Thru")
  {
    return editor.add_node(type, QPointF(100 * graph->get_nodes().size(), 50));
  }

  nlohmann::json copy() const
  {
    auto json = view.json_to();
    for (auto &node : json["nodes"])
      node["settings"] = graph->get_node_ref_by_id<BaseNode>(node["id"])->json_to();
    return json;
  }
};
} // namespace

class GraphEditorTest : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void proxy_preserves_node_identity_and_port_values()
  {
    Fixture     f;
    const auto  id = f.add();
    auto       *node = f.graph->get_node_ref_by_id<BaseNode>(id);
    const auto *proxy = f.view.get_graphics_node_by_id(id)->get_proxy_ref();
    QVERIFY(dynamic_cast<const HesiodNodeProxy *>(proxy));
    QCOMPARE(proxy->get_id(), id);
    QCOMPARE(proxy->get_caption(), std::string("Thru"));
    QCOMPARE(proxy->get_category(), node->get_category());
    QCOMPARE(proxy->get_tool_tip_text(), node->get_documentation_short_html());
    QCOMPARE(proxy->get_nports(), 2);
    QCOMPARE(proxy->get_port_id(0), std::string("input"));
    QCOMPARE(proxy->get_port_id(1), std::string("output"));
    QCOMPARE(proxy->get_port_caption(0), std::string("input"));
    QCOMPARE(proxy->get_port_caption(1), std::string("output"));
    QCOMPARE(proxy->get_port_type(0), gngui::PortType::IN);
    QCOMPARE(proxy->get_port_type(1), gngui::PortType::OUT);
    QCOMPARE(proxy->get_data_type(1), std::string(typeid(hmap::VirtualArray).name()));
    QCOMPARE(proxy->get_data_ref(1), node->get_value_ref_void(1));
    node->set_comment("Updated comment");
    QCOMPARE(proxy->get_comment(), std::string("Updated comment"));
    QCOMPARE(f.view.json_to()["nodes"][0]["id"].get<std::string>(), id);
    QCOMPARE(node->json_to()["id"].get<std::string>(), id);
  }

  void proxy_does_not_extend_model_lifetime()
  {
    QObject owner;
    auto    node = std::make_shared<BaseNode>();
    node->set_id("before");
    auto *proxy = new HesiodNodeProxy(node, &owner);
    proxy->set_id("after");
    QCOMPARE(node->get_id(), std::string("after"));
    std::weak_ptr<BaseNode> weak = node;
    node.reset();
    QVERIFY(weak.expired());
    proxy->set_id("expired");
    QVERIFY(proxy->get_id().empty());
    QVERIFY(proxy->get_caption().empty());
    QVERIFY(proxy->get_category().empty());
    QVERIFY(proxy->get_comment().empty());
    QVERIFY(proxy->get_tool_tip_text().empty());
    QCOMPARE(proxy->get_nports(), 0);
    QVERIFY(proxy->get_port_id(0).empty());
    QVERIFY(proxy->get_port_caption(0).empty());
    QVERIFY(proxy->get_data_type(0).empty());
    QCOMPARE(proxy->get_port_type(0), gngui::PortType::IN);
    QVERIFY(proxy->get_data_ref(0) == nullptr);
  }

  void widget_owns_its_node_proxies()
  {
    auto graph = std::make_shared<GraphNode>("owner", small_config());
    QPointer<const gngui::NodeProxy> proxy;
    {
      GraphNodeWidget widget(graph);
      widget.scene()->setParent(&widget);
      const auto id = widget.on_new_node_request("Thru", {});
      proxy = widget.get_graphics_node_by_id(id)->get_proxy_ref();
      QCOMPARE(proxy->parent(), &widget);
      widget.erase_node(id);
      QVERIFY(proxy);
    }
    QVERIFY(proxy.isNull());
    QCOMPARE(graph->get_nodes().size(), size_t(1));
  }

  void nested_batches_compute_only_after_commit()
  {
    Fixture            f;
    GraphEditor::Batch outer(f.editor);
    const auto         source = f.add("Constant");
    const auto         sink = f.add();
    f.editor.connect({source, "output", sink, "input"});
    QCOMPARE(f.updates, 0);
    QVERIFY(f.computed.empty());
    QVERIFY(f.created.empty());
    outer.commit();
    QCOMPARE(f.updates, 1);
    QCOMPARE(f.computed[source], 1);
    QCOMPARE(f.computed[sink], 1);
    QCOMPARE(f.created.size(), size_t(2));
    QCOMPARE(f.edits, 1);
    QVERIFY(f.notifications_consistent);
    QVERIFY(consistent(*f.graph, f.view));
  }

  void initialized_parameters_are_used_by_the_first_compute()
  {
    Fixture    f;
    const auto id = f.editor.add_node("Constant",
                                      {},
                                      [](BaseNode &node)
                                      { node.set_value<float>("value", 0.75f); });
    auto      *value = f.graph->get_node(id)->get_value_ref<hmap::VirtualArray>("output");
    const auto array = value->to_array(f.config->cm_cpu);
    QCOMPARE(array(0, 0), 0.75f);
    QCOMPARE(f.computed[id], 1);
  }

  void rejected_connection_preserves_input_data_and_scene_data()
  {
    QTest::addColumn<int>("reason");
    QTest::newRow("missing-port") << 0;
    QTest::newRow("wrong-direction") << 1;
    QTest::newRow("wrong-type") << 2;
    QTest::newRow("cycle") << 3;
    QTest::newRow("missing-node") << 4;
  }

  void rejected_connection_preserves_input_data_and_scene()
  {
    QFETCH(int, reason);
    Fixture    f;
    const auto a = f.add();
    const auto b = f.add();
    const auto c = f.add();
    f.editor.connect({a, "output", b, "input"});
    f.editor.connect({b, "output", c, "input"});
    // Add a deliberately different data type before constructing its graphics.
    const auto wrong = f.editor.add_node(
        "Thru",
        {},
        [](BaseNode &node) { node.add_port<float>(gnode::PortType::OUT, "float"); });
    auto      *input = f.graph->get_node(b)->get_value_ref<hmap::VirtualArray>("input");
    const auto before = f.view.json_to();
    f.reset_counts();
    GraphEditor::Link link{c, "output", b, "input"};
    if (reason == 0)
      link.port_out = "missing";
    if (reason == 1)
      link.port_out = "input";
    if (reason == 2)
    {
      link.node_out = wrong;
      link.port_out = "float";
    }
    if (reason == 4)
      link.node_out = "missing";
    QVERIFY_EXCEPTION_THROWN(f.editor.connect(link), std::invalid_argument);
    QVERIFY(f.view.json_to() == before);
    QCOMPARE(f.edits, 0);
    QCOMPARE(f.graph->get_node(b)->get_value_ref<hmap::VirtualArray>("input"), input);
    QCOMPARE(f.updates, 0);
    QVERIFY(consistent(*f.graph, f.view));
  }

  void connection_replacement_and_duplicate()
  {
    Fixture    f;
    const auto a = f.add(), b = f.add(), c = f.add();
    f.editor.connect({a, "output", b, "input"});
    f.reset_counts();
    QVERIFY(f.editor.connect({c, "output", b, "input"}));
    QCOMPARE(f.updates, 1);
    QCOMPARE(f.graph->get_links().size(), size_t(1));
    QCOMPARE(f.graph->get_node(b)->get_value_ref<hmap::VirtualArray>("input"),
             f.graph->get_node(c)->get_value_ref<hmap::VirtualArray>("output"));
    QVERIFY(!f.editor.connect({c, "output", b, "input"}));
    QCOMPARE(f.updates, 1);
    QCOMPARE(f.edits, 1);
    QVERIFY(consistent(*f.graph, f.view));
  }

  void deletion_batches_shared_links_and_recomputes_survivors()
  {
    Fixture    f;
    const auto a = f.add(), b = f.add(), c = f.add();
    f.editor.connect({a, "output", b, "input"});
    f.editor.connect({b, "output", c, "input"});
    f.reset_counts();
    QSignalSpy legacy_nodes(&f.view, &gngui::GraphViewer::node_deleted);
    QSignalSpy legacy_links(&f.view, &gngui::GraphViewer::connection_deleted);
    f.editor.erase({a, b, a}, {{a, "output", b, "input"}});
    QCOMPARE(f.updates, 1);
    QCOMPARE(f.deleted.size(), size_t(2));
    QCOMPARE(f.computed.size(), size_t(1));
    QCOMPARE(f.computed[c], 1);
    QVERIFY(!f.graph->get_node(c)->get_value_ref<hmap::VirtualArray>("input"));
    QCOMPARE(legacy_nodes.count(), 0);
    QCOMPARE(legacy_links.count(), 0);
    QVERIFY(f.notifications_consistent);
    QVERIFY(consistent(*f.graph, f.view));
  }

  void replacement_reconnects_once_and_preserves_position()
  {
    Fixture    f;
    const auto a = f.add(), b = f.add(), c = f.add();
    f.editor.connect({a, "output", b, "input"});
    f.editor.connect({b, "output", c, "input"});
    const auto position = f.view.get_graphics_node_by_id(b)->pos();
    f.reset_counts();
    const auto replacement = f.editor.replace_node(b, "Thru");
    QVERIFY(!f.graph->get_node(b));
    QCOMPARE(f.created, std::vector<std::string>{replacement});
    QCOMPARE(f.deleted, std::vector<std::string>{b});
    QCOMPARE(f.updates, 1);
    QCOMPARE(f.computed[replacement], 1);
    QCOMPARE(f.computed[c], 1);
    QCOMPARE(f.view.get_graphics_node_by_id(replacement)->pos(), position);
    QCOMPARE(f.graph->get_links().size(), size_t(2));
    QVERIFY(f.notifications_consistent);
    QVERIFY(consistent(*f.graph, f.view));
  }

  void replacement_disconnects_incompatible_ports_and_updates_old_downstream()
  {
    Fixture    f;
    const auto a = f.add(), b = f.add();
    f.editor.connect({a, "output", b, "input"});
    f.reset_counts();
    const auto replacement = f.editor.replace_node(a, "Debug");
    QCOMPARE(f.updates, 1);
    QCOMPARE(f.computed[b], 1);
    QVERIFY(!f.graph->get_node(b)->get_value_ref<hmap::VirtualArray>("input"));
    QVERIFY(f.graph->get_node(replacement));
    QVERIFY(consistent(*f.graph, f.view));
  }

  void failed_presentation_preserves_original_and_does_not_poison_outer_batch()
  {
    Fixture    f;
    const auto a = f.add(), b = f.add();
    f.editor.connect({a, "output", b, "input"});
    const auto before = f.view.json_to();
    f.reset_counts();
    GraphEditor::Batch outer(f.editor);
    f.editor.request_update({a});
    f.fail_presentation = true;
    QVERIFY_EXCEPTION_THROWN(f.editor.replace_node(a, "Thru"), std::runtime_error);
    f.fail_presentation = false;
    QVERIFY(f.view.json_to() == before);
    QCOMPARE(f.updates, 0);
    QVERIFY(f.created.empty());
    QVERIFY(f.deleted.empty());
    outer.commit();
    QCOMPARE(f.updates, 1);
    QCOMPARE(f.computed[a], 1);
    QCOMPARE(f.computed[b], 1);
    QVERIFY(consistent(*f.graph, f.view));
  }

  void failed_reconnection_rolls_back_the_replacement()
  {
    Fixture    f;
    const auto a = f.add(), b = f.add(), c = f.add();
    f.editor.connect({a, "output", b, "input"});
    f.editor.connect({b, "output", c, "input"});
    const auto before = f.view.json_to();
    f.reset_counts();
    f.fail_reconnection = true;
    QVERIFY_EXCEPTION_THROWN(f.editor.replace_node(b, "Thru"), std::runtime_error);
    f.fail_reconnection = false;
    QVERIFY(f.view.json_to() == before);
    QVERIFY(consistent(*f.graph, f.view));
    QCOMPARE(f.updates, 0);
    QVERIFY(f.created.empty() && f.deleted.empty());
    QCOMPARE(f.edits, 0);
    QVERIFY(!f.editor.replace_node(b, "Thru").empty());
    QCOMPARE(f.updates, 1);
  }

  void insertion_preserves_fanout()
  {
    Fixture    f;
    const auto a = f.add(), b = f.add(), c = f.add();
    f.editor.connect({a, "output", b, "input"});
    f.editor.connect({a, "output", c, "input"});
    f.reset_counts();
    const auto inserted = f.editor.insert_node(a, "Thru", {12, 34});
    QCOMPARE(f.graph->get_links().size(), size_t(3));
    QCOMPARE(f.updates, 1);
    QCOMPARE(f.computed[inserted], 1);
    QCOMPARE(f.computed[b], 1);
    QCOMPARE(f.computed[c], 1);
    QCOMPARE(f.graph->get_node(b)->get_value_ref<hmap::VirtualArray>("input"),
             f.graph->get_node(inserted)->get_value_ref<hmap::VirtualArray>("output"));
    QVERIFY(consistent(*f.graph, f.view));
  }

  void insertion_keeps_branches_it_cannot_reroute()
  {
    Fixture    f;
    const auto a = f.add(), b = f.add();
    f.editor.connect({a, "output", b, "input"});
    f.reset_counts();
    const auto inserted = f.editor.insert_node(a, "Debug", {});
    QCOMPARE(f.graph->get_links().size(), size_t(2));
    QCOMPARE(f.graph->get_node(b)->get_value_ref<hmap::VirtualArray>("input"),
             f.graph->get_node(a)->get_value_ref<hmap::VirtualArray>("output"));
    QVERIFY(f.graph->get_node(inserted)->get_value_ref<hmap::VirtualArray>("input"));
    QCOMPARE(f.updates, 1);
    QVERIFY(consistent(*f.graph, f.view));
  }

  void paste_remaps_ids_and_computes_copies_only()
  {
    Fixture    f;
    const auto a = f.add(), b = f.add();
    f.editor.connect({a, "output", b, "input"});
    auto input = f.copy();
    input["links"][0]["port_out_id"] = "out";
    input["links"][0]["port_in_id"] = "in";
    f.reset_counts();
    const auto pasted = f.editor.import_nodes(input, {20, 40});
    QCOMPARE(f.graph->get_nodes().size(), size_t(4));
    QCOMPARE(f.graph->get_links().size(), size_t(2));
    QCOMPARE(f.updates, 1);
    QCOMPARE(f.computed.size(), size_t(2));
    QVERIFY(!f.computed.contains(a));
    QVERIFY(!f.computed.contains(b));
    const auto from = pasted["links"][0]["node_out_id"].get<std::string>();
    const auto to = pasted["links"][0]["node_in_id"].get<std::string>();
    QVERIFY(from != a && to != b);
    QVERIFY(f.computed.contains(from) && f.computed.contains(to));
    QCOMPARE(pasted["links"][0]["port_out_id"].get<std::string>(), std::string("output"));
    QCOMPARE(pasted["links"][0]["port_in_id"].get<std::string>(), std::string("input"));
    QVERIFY(f.notifications_consistent);
    QVERIFY(consistent(*f.graph, f.view));
  }

  void invalid_paste_rolls_back_created_nodes()
  {
    Fixture    f;
    const auto a = f.add(), b = f.add();
    f.editor.connect({a, "output", b, "input"});
    auto input = f.copy();
    input["links"][0]["port_in_id"] = "nonexistent";
    const auto before = f.view.json_to();
    f.reset_counts();
    QVERIFY_EXCEPTION_THROWN(f.editor.import_nodes(input, {}), std::invalid_argument);
    QCOMPARE(f.graph->get_nodes().size(), size_t(2));
    QVERIFY(f.view.json_to() == before);
    QCOMPARE(f.updates, 0);
    QVERIFY(f.created.empty() && f.deleted.empty());
    QCOMPARE(f.edits, 0);
    f.editor.request_update({a});
    QCOMPARE(f.updates, 1);
    QVERIFY(consistent(*f.graph, f.view));
  }

  void clear_removes_model_nodes_and_broadcast_registration()
  {
    auto    manager = std::make_shared<GraphManager>("graphs");
    Fixture f;
    manager->add_graph_node(f.graph, "test");
    f.add("Broadcast");
    f.add("Receive");
    QCOMPARE(manager->get_broadcast_params().size(), size_t(1));
    f.reset_counts();
    f.editor.clear();
    QVERIFY(f.graph->get_nodes().empty());
    QVERIFY(manager->get_broadcast_params().empty());
    QCOMPARE(f.deleted.size(), size_t(2));
    QCOMPARE(f.updates, 0);
    QVERIFY(f.notifications_consistent);
    QVERIFY(consistent(*f.graph, f.view));
  }

  void computation_failure_does_not_leave_edits_blocked()
  {
    Fixture    f;
    const auto id = f.add();
    f.graph->update_started = []()
    { throw std::runtime_error("Injected update failure"); };
    QVERIFY_EXCEPTION_THROWN(f.editor.request_update({id}), std::runtime_error);
    f.graph->update_started = [&]() { ++f.updates; };
    f.reset_counts();
    f.editor.request_update({id});
    QCOMPARE(f.updates, 1);
    QCOMPARE(f.computed[id], 1);
  }

  void loading_an_existing_model_is_presentation_only_data()
  {
    QTest::addColumn<QString>("target_type");
    QTest::addColumn<QString>("output_port");
    QTest::addColumn<QString>("input_port");
    QTest::newRow("current-port") << QString("Thru") << QString("output")
                                 << QString("input");
    QTest::newRow("legacy-port") << QString("Thru") << QString("out")
                                << QString("in");
    QTest::newRow("post-process-current-port")
        << QString("PostProcess") << QString("output") << QString("input");
    QTest::newRow("post-process-legacy-port")
        << QString("PostProcess") << QString("output") << QString("in");
  }

  void loading_an_existing_model_is_presentation_only()
  {
    QFETCH(QString, target_type);
    QFETCH(QString, output_port);
    QFETCH(QString, input_port);
    Fixture    f;
    const auto a = f.add(), b = f.add(target_type.toStdString());
    const auto input = f.graph->get_node(b)->get_port_label(0);
    f.editor.connect({a, "output", b, input});
    auto saved = f.view.json_to();
    saved["links"][0]["port_out_id"] = output_port.toStdString();
    saved["links"][0]["port_in_id"] = input_port.toStdString();
    auto model = f.graph->json_to();
    model["links"][0]["port_id_from"] = output_port.toStdString();
    model["links"][0]["port_id_to"] = input_port.toStdString();
    auto loaded = std::make_shared<GraphNode>("loaded", f.config);
    loaded->json_from(model);
    GraphNodeWidget widget(loaded);
    widget.scene()->setParent(&widget);
    QSignalSpy edits(&widget, &GraphNodeWidget::graph_edited);
    QSignalSpy updates(&widget, &GraphNodeWidget::update_started);
    widget.json_from(saved);
    QCOMPARE(edits.count(), 0);
    QCOMPARE(updates.count(), 0);
    QCOMPARE(loaded->get_links().size(), size_t(1));
    QVERIFY(consistent(*loaded, widget));

    const auto saved_model = loaded->json_to();
    const auto saved_widget = widget.json_to();
    QVERIFY(saved_model["links"][0]["port_id_to"] == "input");
    QVERIFY(saved_widget["links"][0]["port_in_id"] == "input");
    auto reloaded = std::make_shared<GraphNode>("reloaded", f.config);
    reloaded->json_from(saved_model);
    GraphNodeWidget reloaded_widget(reloaded);
    reloaded_widget.scene()->setParent(&reloaded_widget);
    reloaded_widget.json_from(saved_widget);
    QCOMPARE(reloaded->get_links().size(), size_t(1));
    QVERIFY(consistent(*reloaded, reloaded_widget));
    reloaded_widget.erase_node(a);
    reloaded_widget.erase_node(b);
    widget.erase_node(a);
    widget.erase_node(b);
  }

  void drag_to_create_is_one_edit_data()
  {
    QTest::addColumn<QString>("port");
    QTest::newRow("from-output") << QString("output");
    QTest::newRow("from-input") << QString("input");
  }

  void drag_to_create_is_one_edit()
  {
    QFETCH(QString, port);
    auto            graph = std::make_shared<GraphNode>("drag", small_config());
    GraphNodeWidget widget(graph);
    widget.scene()->setParent(&widget);
    const auto original = widget.on_new_node_request("Thru", {});
    QSignalSpy updates(&widget, &GraphNodeWidget::update_started);
    QSignalSpy edits(&widget, &GraphNodeWidget::graph_edited);
    QTimer     timeout;
    timeout.setSingleShot(true);
    QObject::connect(
        &timeout,
        &QTimer::timeout,
        [&]()
        {
          if (auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget()))
            menu->close();
        });
    timeout.start(2000);
    QTimer::singleShot(0,
                       &widget,
                       [&]()
                       {
                         auto *menu = qobject_cast<QMenu *>(
                             QApplication::activePopupWidget());
                         QVERIFY(menu);
                         auto *filter = menu->findChild<QLineEdit *>();
                         QVERIFY(filter);
                         QTest::keyClicks(filter, "Thru");
                         for (auto *action : menu->actions())
                           if (action->text() == "Thru")
                           {
                             menu->setActiveAction(action);
                             QTest::keyClick(menu, Qt::Key_Return);
                             return;
                           }
                         QFAIL("The node menu did not offer Thru");
                       });
    widget.on_connection_dropped(original, port.toStdString(), {});
    timeout.stop();
    QCOMPARE(graph->get_nodes().size(), size_t(2));
    QCOMPARE(graph->get_links().size(), size_t(1));
    QCOMPARE(updates.count(), 1);
    QCOMPARE(edits.count(), 1);
    QVERIFY(consistent(*graph, widget));
    widget.clear_all();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }

  void widget_gestures_and_duplicate_use_the_editor()
  {
    auto            graph = std::make_shared<GraphNode>("widget", small_config());
    GraphNodeWidget widget(graph);
    widget.scene()->setParent(&widget);
    int updates = 0;
    QObject::connect(&widget, &GraphNodeWidget::update_started, [&]() { ++updates; });
    QSignalSpy deleted(&widget, &GraphNodeWidget::node_deleted);
    QSignalSpy edits(&widget, &GraphNodeWidget::graph_edited);
    const auto a = widget.on_new_node_request("Thru", {0, 0});
    const auto b = widget.on_new_node_request("Thru", {400, 0});
    auto      *from = widget.get_graphics_node_by_id(a);
    auto      *to = widget.get_graphics_node_by_id(b);
    updates = 0;
    edits.clear();
    from->connection_started(from, 1);
    from->connection_finished(from, 1, to, 0);
    QCOMPARE(updates, 1);
    QCOMPARE(edits.count(), 1);
    // The widget catches a rejected cyclic drag without changing either side.
    to->connection_started(to, 1);
    to->connection_finished(to, 1, from, 0);
    QCOMPARE(updates, 1);
    QCOMPARE(edits.count(), 1);
    QVERIFY(consistent(*graph, widget));
    widget.deselect_all();
    widget.set_node_as_selected(a);
    updates = 0;
    const auto replacement = widget.on_new_node_request_replace("Thru");
    QVERIFY(!replacement.empty());
    QCOMPARE(deleted.count(), 1);
    QCOMPARE(updates, 1);
    QVERIFY(consistent(*graph, widget));
    updates = 0;
    widget.on_nodes_duplicate_request({replacement, b},
                                      {widget.get_graphics_node_by_id(replacement)->pos(),
                                       widget.get_graphics_node_by_id(b)->pos()});
    QCOMPARE(updates, 1);
    QCOMPARE(graph->get_nodes().size(), size_t(4));
    QVERIFY(consistent(*graph, widget));
    widget.clear_all();
    QVERIFY(graph->get_nodes().empty());
    QVERIFY(consistent(*graph, widget));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }
};

int main(int argc, char **argv)
{
  // Use the real application context and node factory, but skip main-window and
  // OpenGL viewer creation. Isolate per-user configuration for the test process.
  QTemporaryDir config_dir;
  qputenv("XDG_CONFIG_HOME", config_dir.path().toUtf8());
  qputenv("XDG_CACHE_HOME", config_dir.path().toUtf8());
  qputenv("QT_LOGGING_RULES", HESIOD_QPUTENV_QT_LOGGING_RULES);
  HesiodApplication app(argc, argv, HesiodApplication::StartupMode::ContextOnly);
  app.get_context().app_settings.interface.enable_node_settings_in_node_body = false;
  GN_STYLE->viewer.add_toolbar = false;
  GraphEditorTest tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "test_graph_editor.moc"
