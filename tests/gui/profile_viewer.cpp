/* Copyright (c) 2026. Distributed under the GNU General Public License.
 * See LICENSE. */
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <QMouseEvent>
#include <QOpenGLFunctions>
#include <QTemporaryDir>
#include <QtTest>
#include <omp.h>

#include "gnodegui/style.hpp"
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph/data_preview.hpp"
#include "hesiod/gui/widgets/graph/graph_node_widget.hpp"
#include "hesiod/gui/widgets/viewers/viewer_3d.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/profiling.hpp"

using namespace hesiod;

// Explicit GUI workload, not an offscreen CTest or a frame-rate benchmark.
class ProfileViewer : public Viewer3D
{
public:
  using Viewer3D::Viewer3D;
  void show_heightmap_layers()
  {
    view_param.port_ids["elevation"] = "output";
    view_param.port_ids["water_depth"] = "output";
    view_param.port_ids["color"] = "output";
    update_widgets();
  }
};

int main(int argc, char **argv)
{
#ifdef HESIOD_ENABLE_TRACY
  if (const char *wait = std::getenv("HESIOD_TRACY_WAIT");
      wait && std::string_view(wait) == "1")
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!TracyIsConnected && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!TracyIsConnected)
      return 2;
  }
#endif
  QTemporaryDir config_dir;
  qputenv("XDG_CONFIG_HOME", config_dir.path().toUtf8());
  qputenv("XDG_CACHE_HOME", config_dir.path().toUtf8());
  qputenv("QT_LOGGING_RULES", HESIOD_QPUTENV_QT_LOGGING_RULES);
  HesiodApplication app(argc, argv, HesiodApplication::StartupMode::ContextOnly);
  [[maybe_unused]] ProfileMemorySampler sampler;
  omp_set_num_threads(1);
  auto &settings = app.get_context().app_settings;
  settings.interface.enable_node_settings_in_node_body = false;
  settings.viewer.mesh_resolution = 256;
  settings.node_editor.preview_w = 128;
  settings.node_editor.preview_h = 128;
  GN_STYLE->viewer.add_toolbar = false;

  auto config = std::make_shared<GraphConfig>();
  config->set_shape({512, 512});
  config->set_tiling({1, 1});
  config->set_overlap(0.f);
  config->storage_mode = hmap::StorageMode::VA_RAM;
  auto            graph = std::make_shared<GraphNode>("viewer-profile", config);
  const auto      id = graph->add_node("GaussianPulse");
  auto           *node = graph->get_node_ref_by_id<BaseNode>(id);
  GraphNodeWidget graph_widget(graph);
  graph_widget.scene()->setParent(&graph_widget);
  ProfileViewer viewer(&graph_widget);
  viewer.setWindowTitle("Hesiod viewer profiling workload");
  viewer.resize(960, 640);
  viewer.show();
  if (!QTest::qWaitForWindowExposed(&viewer, 10000))
    return 3;
  auto *renderer = viewer.findChild<qtr::RenderWidget *>();
  if (!renderer || !renderer->isValid())
    return 4; // Do not report an offscreen/failed OpenGL workload as measured.
  DataPreview preview(node->get_shared());
  {
    HSD_PROFILE_ZONE("Viewer workload::initial_selection");
    graph->update();
    viewer.set_current_node_id(id);
    viewer.show_heightmap_layers();
    preview.update_preview();
    QTest::qWait(1000); // Includes event-loop settling, not a computation timer.
  }
  if (!renderer->grabFramebuffer().save("viewer-profile-initial.png"))
    return 5;
  for (int repeat = 0; repeat < 3; ++repeat)
  {
    HSD_PROFILE_ZONE("Viewer workload::parameter_change");
    node->set_value<float>("radius", 0.2f + 0.05f * repeat);
    graph->update(id);
    preview.update_preview();
    QTest::qWait(1000);
  }
  {
    HSD_PROFILE_ZONE("Viewer workload::camera_drag");
    const QPoint start(renderer->width() / 2, renderer->height() / 2);
    QTest::mousePress(renderer, Qt::LeftButton, Qt::NoModifier, start);
    for (int i = 1; i <= 20; ++i)
    {
      const QPoint local = start + QPoint(3 * i, i);
      QMouseEvent  move(QEvent::MouseMove,
                       local,
                       renderer->mapToGlobal(local),
                       Qt::NoButton,
                       Qt::LeftButton,
                       Qt::NoModifier);
      QCoreApplication::sendEvent(renderer, &move);
      QTest::qWait(30);
    }
    QTest::mouseRelease(renderer, Qt::LeftButton, Qt::NoModifier, start + QPoint(60, 20));
    QTest::qWait(500);
  }
  {
    HSD_PROFILE_ZONE("Viewer workload::render_2d");
    viewer.set_render_type(0);
    for (int frame = 0; frame < 6; ++frame)
    {
      renderer->update();
      QTest::qWait(50);
    }
    viewer.set_render_type(1);
  }
  // Exercise context switching, destruction and replacement while the first
  // renderer stays alive. Each context must own its own timer queries.
  for (int lifetime = 0; lifetime < 2; ++lifetime)
  {
    HSD_PROFILE_ZONE("Viewer workload::multiple_contexts");
    ProfileViewer secondary(&graph_widget);
    secondary.resize(640, 480);
    secondary.show();
    if (!QTest::qWaitForWindowExposed(&secondary, 10000))
      return 6;
    auto *other = secondary.findChild<qtr::RenderWidget *>();
    if (!other || !other->isValid())
      return 6;
    secondary.set_current_node_id(id);
    secondary.show_heightmap_layers();
    for (int frame = 0; frame < 12; ++frame)
    {
      renderer->update();
      other->update();
      QTest::qWait(50);
    }
    other->makeCurrent();
    const GLenum error = other->context()->functions()->glGetError();
    other->doneCurrent();
    if (error != GL_NO_ERROR)
      return 7;
    secondary.close();
  }
  for (int frame = 0; frame < 8; ++frame)
  {
    renderer->update();
    QTest::qWait(50);
  }
  renderer->makeCurrent();
  const GLenum error = renderer->context()->functions()->glGetError();
  renderer->doneCurrent();
  if (error != GL_NO_ERROR)
    return 7;

  if (preview.get_preview_pixmap().isNull() ||
      !preview.get_preview_pixmap().save("viewer-preview.png") ||
      !renderer->grabFramebuffer().save("viewer-profile.png"))
    return 5;
  std::cout << "Viewer workload complete: 512x512, mesh cap 256, 3 parameter changes, "
               "camera drag, 2D rendering, and two secondary-context lifetimes.\n";
  viewer.close();
  return 0;
}
