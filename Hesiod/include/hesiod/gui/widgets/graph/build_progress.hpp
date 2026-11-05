/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <functional>

#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QWidget>

namespace hesiod
{

namespace rail
{
class ToolRail;
} // namespace rail

// The update of a graph, at the bottom of its editor: a bar filling with the
// nodes computed while it is updated, then a red "Graph needs update" while an
// update held with Esc leaves it partly up to date (a click goes on with it).
// Hidden otherwise.
//
// The nodes are counted over the whole update, each once: an update that is
// cancelled by an edit, or held, and goes on again keeps what it had done.
class BuildProgress : public QWidget
{
public:
  // over `area`, a sibling of it
  BuildProgress(QWidget *area, std::function<void()> resume);

  // see GraphNode's update callbacks
  void on_update_started();
  void on_update_steps(const QStringList &pass, int done); // see GraphNode
  void on_update_held(bool held);
  void on_update_finished();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void place();
  void refresh();

  QPointer<QWidget>        area;
  QPointer<rail::ToolRail> rail; // the toolbar of the editor, kept clear of
  std::function<void()>    resume;
  bool                     updating = false;
  bool                     held = false;
  QSet<QString>            done_ids;        // over the whole update
  QStringList              pass;            // the nodes of the current pass
  int                      done_before = 0; // done, and not in the current pass
  int                      done = 0;
  int                      total = 0;
};

} // namespace hesiod
