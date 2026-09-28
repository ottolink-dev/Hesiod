/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QPointer>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/documentation_popup.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/menu_chrome.hpp"
#include "hesiod/gui/widgets/viewers/viewer.hpp"
#include "hesiod/model/graph/graph_node.hpp"

namespace hesiod
{
namespace
{
// Copy / Paste properties: one buffer for the session, pasted only onto a node
// of the same type
struct PropertiesClipboard
{
  std::string    node_type;
  nlohmann::json settings;
};

PropertiesClipboard &properties_clipboard()
{
  static PropertiesClipboard clipboard;
  return clipboard;
}
} // namespace

void GraphNodeWidget::set_preview_viewer(Viewer *viewer)
{
  this->preview_viewer = viewer;
}

void GraphNodeWidget::show_node_context_menu(const std::string &node_id,
                                             const QPoint      &global_pos)
{
  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(node_id);
  if (!p_node)
    return;

  const std::string node_type = p_node->get_label();
  const bool locked = this->preview_viewer && this->preview_viewer->is_locked_on(node_id);

  HsdMenu  menu(QString::fromStdString(node_type), this);
  QAction *settings = menu.addAction(HSD_ICON("tune"), "Settings…");
  menu.addSeparator();
  QAction *refresh = menu.addAction(HSD_ICON("refresh"), "Refresh");
  QAction *reset = menu.addAction(HSD_ICON("settings_backup_restore"), "Reset Settings");
  QAction *lock = menu.addAction(HSD_ICON("push_pin"),
                                 locked ? "Unlock Preview" : "Lock Preview");
  lock->setEnabled(this->preview_viewer != nullptr);
  menu.addSeparator();
  QAction *copy_props = menu.addAction("Copy Properties");
  QAction *paste_props = menu.addAction("Paste Properties");
  paste_props->setEnabled(properties_clipboard().node_type == node_type);
  menu.addSeparator();
  QAction *duplicate = menu.addAction("Duplicate\tCtrl+D");
  QAction *copy = menu.addAction("Copy\tCtrl+C");
  QAction *remove = menu.addAction("Delete\tDel");
  menu.addSeparator();
  QAction *info = menu.addAction(HSD_ICON("info"), "Info and Comment…");
  QAction *help = menu.addAction(HSD_ICON("help"), "Help");

  QPointer<GraphNodeWidget> guard(this);
  QAction                  *chosen = menu.exec(global_pos);

  // the menu runs its own event loop: look everything up again
  if (!chosen || !guard || !(gno = this->p_graph_node.lock()))
    return;
  p_node = gno->get_node_ref_by_id<BaseNode>(node_id);
  if (!p_node)
    return;

  meta::AttributeContainer &container = p_node->get_meta_group().current();

  // Duplicate, Copy and Delete act on the whole selection when the node is part
  // of it, as the keyboard shortcuts do
  std::vector<QPointF>     positions;
  std::vector<std::string> ids = this->get_selected_node_ids(&positions);
  if (std::find(ids.begin(), ids.end(), node_id) == ids.end())
  {
    ids = {node_id};
    positions = {this->get_graphics_node_by_id(node_id)->pos()};
  }

  if (chosen == settings)
    this->show_node_settings_popup(node_id, global_pos);
  else if (chosen == refresh)
    this->update_graph_model(node_id);
  else if (chosen == reset && !p_node->get_initial_meta_state().empty())
  {
    container.json_from(p_node->get_initial_meta_state(), true);
    this->update_graph_model(node_id);
  }
  else if (chosen == lock && this->preview_viewer)
    this->preview_viewer->toggle_lock_on(node_id);
  else if (chosen == copy_props)
    properties_clipboard() = {node_type, container.json_to()};
  else if (chosen == paste_props)
  {
    container.json_from(properties_clipboard().settings, true);
    this->update_graph_model(node_id);
  }
  else if (chosen == duplicate)
    this->on_nodes_duplicate_request(ids, positions);
  else if (chosen == copy)
    this->on_nodes_copy_request(ids, positions);
  else if (chosen == remove)
    this->request_deletion(ids, {});
  else if (chosen == info)
    this->on_node_info(node_id);
  else if (chosen == help)
  {
    auto *popup = new DocumentationPopup(node_type, p_node->get_documentation_html());
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->show();
  }
}

} // namespace hesiod
