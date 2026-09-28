/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "gnodegui/graph_viewer.hpp"

namespace hesiod
{
class BaseNode;
class GraphNode;

// Coordinates topology edits and synchronization with the existing GraphViewer.
class GraphEditor
{
public:
  using Link = gngui::LinkEndpoints;
  struct NodePresentation
  {
    std::function<void(const std::string &, QPointF)> create;
    std::function<void(const std::string &)>          created;
    std::function<void(const std::string &)>          deleted;
    std::function<void()>                             changed;
  };

  GraphEditor(std::weak_ptr<GraphNode> graph,
              gngui::GraphViewer      &view,
              NodePresentation         presentation);

  // The outermost commit computes and publishes deferred notifications.
  // Destruction restores scheduling state without computing; callers must roll
  // back their own topology changes before abandoning a batch.
  class Batch
  {
  public:
    explicit Batch(GraphEditor &editor);
    ~Batch();
    Batch(const Batch &) = delete;
    Batch &operator=(const Batch &) = delete;
    void   commit();

  private:
    GraphEditor          &editor;
    std::set<std::string> previous_dirty;
    bool                  previous_full_update;
    bool                  previous_changed;
    size_t                previous_notifications;
    bool                  active = true;
  };

  std::string    add_node(const std::string                     &type,
                          QPointF                                position,
                          const std::function<void(BaseNode &)> &initialize = {});
  bool           connect(const Link &link);
  bool           disconnect(const Link &link);
  void           erase(const std::vector<std::string> &node_ids,
                       const std::vector<Link>        &links = {});
  void           clear();
  std::string    replace_node(const std::string &id, const std::string &type);
  std::string    insert_node(const std::string &id,
                             const std::string &type,
                             QPointF            position);
  nlohmann::json import_nodes(const nlohmann::json &json, QPointF origin);
  void           request_update(const std::vector<std::string> &ids = {});

private:
  struct Notification
  {
    std::string id;
    bool        created;
  };

  std::shared_ptr<GraphNode> graph() const;
  std::vector<Link>          links_for(const std::string &id) const;
  void                       validate_connection(const Link &link) const;
  bool                       compatible(const Link &link) const;
  void                       finish_batch();
  void                       restore_links(const std::vector<Link> &links);

  std::weak_ptr<GraphNode>  model;
  gngui::GraphViewer       &view;
  NodePresentation          presentation;
  size_t                    batch_depth = 0;
  std::set<std::string>     dirty;
  bool                      full_update = false;
  bool                      changed = false;
  std::vector<Notification> notifications;
};
} // namespace hesiod
