/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <filesystem>

#include <QTimer>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/cli/batch_mode.hpp"
#include "hesiod/gui/project_ui.hpp"
#include "hesiod/gui/widgets/graph_tabs_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod::cli
{

int parse_args(args::ArgumentParser &parser,
               int                   argc,
               char                 *argv[],
               std::string          &startup_file)
{
  args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});

  args::ValueFlag<std::string> file_flag(parser,
                                         "hsd file",
                                         "Project file (.hsd) to open at startup",
                                         {'f', "file"});

  args::Positional<std::string> file_positional(parser,
                                                "file",
                                                "Project file (.hsd) to open at startup");

  args::Group group(parser,
                    "This group is all exclusive:",
                    args::Group::Validators::DontCare);

  args::Flag snapshot_generation(group, "", "Node snapshot generation", {"snapshot"});

  args::Flag node_inventory(group, "", "Node inventory output", {"inventory"});

  args::ValueFlag<std::string> batch(group,
                                     "hsd file",
                                     "Execute Hesiod in batch mode",
                                     {'b', "batch"});

  args::Group batch_args(group,
                         "batch mode arguments",
                         args::Group::Validators::DontCare);

  args::ValueFlag<glm::ivec2> shape_arg(
      batch_args,
      "shape",
      "Heightmap shape (in pixels), ex. --shape=512,512",
      {"shape"});

  args::ValueFlag<glm::ivec2> tiling_arg(batch_args,
                                         "tiling",
                                         "Heightmap tiling, ex. --tiling=4,4",
                                         {"tiling"});

  args::ValueFlag<float> overlap_arg(
      batch_args,
      "overlap",
      "Tile overlapping ratio (in [0, 1[), ex. --overlap=0.25",
      {"overlap"});

  try
  {
    parser.ParseCLI(argc, argv);

    if (batch)
    {
      run_batch_mode(args::get(batch),
                     shape_arg ? args::get(shape_arg) : glm::ivec2(0, 0),
                     tiling_arg ? args::get(tiling_arg) : glm::ivec2(0, 0),
                     overlap_arg ? args::get(overlap_arg) : -1.f);
      return 0;
    }
    else if (snapshot_generation)
    {
      run_snapshot_generation();
      return 0;
    }
    else if (node_inventory)
    {
      run_node_inventory();
      return 0;
    }

    if (file_flag && file_positional &&
        args::get(file_flag) != args::get(file_positional))
    {
      std::cerr << "Error: conflicting project files requested: positional argument is "
                << "'" << args::get(file_positional) << "' but -f/--file is " << "'"
                << args::get(file_flag) << "'. "
                << "Provide only one of them (or make them identical)." << std::endl;
      return 1;
    }

    if (file_flag)
      startup_file = args::get(file_flag);
    else if (file_positional)
      startup_file = args::get(file_positional);
  }
  catch (const args::Help &help)
  {
    std::cout << parser;
    return 0;
  }

  catch (args::Error &e)
  {
    std::cerr << e.what() << std::endl << parser;
    return 1;
  }

  return -1;
}

void run_batch_mode(const std::string &filename,
                    const glm::ivec2  &shape,
                    const glm::ivec2  &tiling,
                    float              overlap,
                    const GraphConfig *p_input_model_config)
{
  Logger::log()->info("executing Hesiod in batch mode");
  Logger::log()->trace("file: {}", filename);
  Logger::log()->trace("cli shape: {{{}, {}}}", shape.x, shape.y);
  Logger::log()->trace("cli tiling: {{{}, {}}}", tiling.x, tiling.y);
  Logger::log()->trace("cli overlap: {}", overlap);

  // define actual computation configuration based on CLI inputs. If
  // nothing is provided, use the configs from the input file but if
  // an input config is provided, this config is used for all the
  // graph nodes.
  hesiod::GraphConfig config;

  // override some parameters on request
  if (p_input_model_config)
  {
    config.cm_cpu.mode = p_input_model_config->cm_cpu.mode;
    config.cm_gpu.mode = p_input_model_config->cm_gpu.mode;

    // force memory release after each node computation
    config.cm_cpu.trim_storage = true;
    config.cm_gpu.trim_storage = true;
    config.cm_single_array.trim_storage = true;
  }

  if (shape.x || shape.y || tiling.x || tiling.y || overlap >= 0.f)
  {
    glm::ivec2 new_shape = (shape.x && shape.y) ? shape : glm::ivec2(1024, 1024);
    glm::ivec2 new_tiling = (tiling.x && tiling.y) ? tiling : glm::ivec2(1, 1);
    float      new_overlap = overlap >= 0.f
                                 ? overlap
                                 : ((config.tiling.x == 1 && config.tiling.y == 1) ? 0.f
                                                                                   : 0.5f);

    config.set_shape(new_shape);
    config.set_tiling(new_tiling);
    config.set_overlap(new_overlap);

    Logger::log()->info("graph configurations will be overriden:");
    Logger::log()->info("compute shape: {{{}, {}}}", config.shape.x, config.shape.y);
    Logger::log()->info("compute tiling: {{{}, {}}}", config.tiling.x, config.tiling.y);
    Logger::log()->info("compute overlap: {}", config.overlap);
  }

  GraphManager graph_manager;
  graph_manager.load_from_file(filename, &config);

  // flatten & export if there is a configuration defined
  if (!graph_manager.get_export_param().export_path.empty())
    graph_manager.export_flatten();
}

void run_node_inventory()
{
  Logger::log()->info("executing Hesiod in node inventory mode");
  hesiod::dump_node_inventory("node_inventory");

  auto config = std::make_shared<hesiod::GraphConfig>();
  hesiod::dump_node_documentation_stub("node_documentation_stub.json", config);

  hesiod::dump_node_settings_screenshots();
}

void run_snapshot_generation()
{
  Logger::log()->info("executing Hesiod in snapshot generation mode");

  std::map<std::string, std::string> inventory = get_node_inventory();

  // TODO hardcoded
  const std::string ex_path = "data/examples/";
  const QSize       size = QSize(512, 512);

  auto *app = static_cast<hesiod::HesiodApplication *>(QCoreApplication::instance());

  AppContext &ctx = app->get_context();
  ctx.save_state();

  // headless: skip GUI-only / OpenGL widgets (3D viewer) when building the graph UI
  ctx.headless = true;

  // strip everything but the node graph from the snapshot: deactivate the node
  // settings pan, the per-graph 3D viewer, the node library panel (so the graph
  // gets the full frame width), and the WebEngine texture downloader.
  ctx.app_settings.node_editor.show_node_settings_pan = false;
  ctx.app_settings.node_editor.show_viewer = false;
  ctx.app_settings.node_editor.show_node_library_pan = false;
  ctx.app_settings.interface.enable_texture_downloader = false;

  for (auto &[node_type, _] : inventory)
  {
    const std::string fname = ex_path + node_type + ".hsd";

    if (std::filesystem::exists(fname))
    {
      Logger::log()->trace("- default file exists: {}", fname);

      // a single broken example (e.g. a stale port name in the .hsd) must not
      // abort the whole batch: log and skip it
      try
      {
        app->load_project_model_and_ui(fname);

        GraphTabsWidget *p_gtw = app->get_project_ui_ref()->get_graph_tabs_widget_ref();

        if (p_gtw)
        {
          QWidget *widget = dynamic_cast<QWidget *>(p_gtw);

          // size the widget to the final render size FIRST, then fit the graph
          // to it: zoom_to_content() must run against the actual 512x512
          // viewport, otherwise the fit is computed at the pre-render size and
          // the graph is cropped (the old "TODO refit" problem).
          widget->setFixedSize(size);
          QCoreApplication::processEvents();

          // the widget tree is never shown in headless mode, so the resize
          // stays pending and child layouts (incl. the graph view viewport)
          // keep their stale pre-load geometry until the first render. grab()
          // flushes pending resize/layout the same way render() does, so the
          // viewport has its final size before the fit is computed.
          widget->grab();

          p_gtw->zoom_to_content();
          QCoreApplication::processEvents();

          render_widget_screenshot(widget, node_type + "_hsd_example.png", size);

          QCoreApplication::processEvents();

          // to avoid Qt panicking...
          QEventLoop loop;
          QTimer::singleShot(50, &loop, &QEventLoop::quit);
          loop.exec();
        }
      }
      catch (const std::exception &e)
      {
        Logger::log()->error("snapshot generation failed for '{}', skipping: {}",
                             fname,
                             e.what());
      }
    }
  }

  ctx.restore_state();
}

} // namespace hesiod::cli
