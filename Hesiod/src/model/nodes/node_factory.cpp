/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "meta_qt/container_group_widget.hpp"
#include <fstream>
#include <stdexcept>

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/utils.hpp"

#include "hesiod/gui/widgets/gui_utils.hpp"

// specific nodes
#include "hesiod/model/nodes/broadcast_node.hpp"
#include "hesiod/model/nodes/receive_node.hpp"

// setup node, create ports, attributes, define compute function,
// define attributes tooltips...
#define SETUP_NODE(NodeType, node_type)                                                  \
  case str2int(#NodeType):                                                               \
    setup_##node_type##_node(*sptr);                                                     \
    sptr->set_compute_fct(&compute_##node_type##_node);                                  \
    sptr->update_attributes_tool_tip();                                                  \
    sptr->finalize_attributes();                                                         \
    break;

namespace hesiod
{

// HELPERS
constexpr unsigned int str2int(const char *str, int h = 0)
{
  // https://stackoverflow.com/questions/16388510
  return !str[h] ? 5381 : (str2int(str, h + 1) * 33) ^ str[h];
}

void dump_node_inventory(const std::string &fname)
{
  Logger::log()->trace("dump_node_inventory");

  std::map<std::string, std::string> ni = hesiod::get_node_inventory();

  // --- export to mermaid also

  std::fstream f;
  f.open(fname + ".csv", std::ios::out);

  for (auto [name, category] : ni)
  {
    f << name << ";";
    for (auto v : split_string(category, '/'))
      f << v << ";";
    f << "\n";
  }

  f.close();

  // --- export to mermaid also

  f.open(fname + ".mmd", std::ios::out);

  f << "---\n";
  f << "title: Hesiod nodes\n";
  f << "---\n";
  f << "flowchart LR\n";

  std::vector<std::string> edges = {};

  for (auto [name, category] : ni)
  {
    std::vector<std::string> categories = split_string(category, '/');
    categories.insert(categories.begin(), "NODES[(NODES)]");
    categories.push_back("#" + name + "([" + name + "])");

    for (size_t k = 0; k < categories.size() - 1; k++)
    {
      std::string new_edge = categories[k] + " --> " + categories[k + 1];

      if (std::find(edges.begin(), edges.end(), new_edge) == edges.end())
      {
        edges.push_back(new_edge);
        f << new_edge << "\n";
      }
    }

    f << "style " << "#" + name << " fill:#f9f\n";
  }

  f.close();
}

void dump_node_documentation_stub(const std::string         &fname,
                                  std::weak_ptr<GraphConfig> config)
{
  Logger::log()->trace("dump_node_documentation_stub");

  std::map<std::string, std::string> ni = hesiod::get_node_inventory();

  nlohmann::json json;
  for (auto [name, category] : ni)
  {
    std::shared_ptr<gnode::Node> p_node = node_factory(name, config);
    hesiod::BaseNode *p_base_node = dynamic_cast<hesiod::BaseNode *>(p_node.get());
    json[name] = p_base_node->node_parameters_to_json();
  }

  std::fstream f;
  f.open(fname, std::ios::out);
  f << json.dump(4) << "\n";
  f.close();
}

void dump_node_settings_screenshots()
{
  Logger::log()->trace("dump_node_settings_screenshots");

  // use default, not important
  auto config = std::make_shared<hesiod::GraphConfig>();

  std::map<std::string, std::string> ni = hesiod::get_node_inventory();

  nlohmann::json json;
  for (auto [name, category] : ni)
  {
    std::shared_ptr<gnode::Node> p_node = node_factory(name, config);
    hesiod::BaseNode *p_base_node = dynamic_cast<hesiod::BaseNode *>(p_node.get());

    QWidget *widget = new meta::qt::ContainerGroupWidget(
        p_base_node->get_meta_group(),
        meta::qt::ContainerRenderOptions{},
        nullptr);

    render_widget_screenshot(widget, p_base_node->get_label() + "_settings.png", QSize());
  }
}

std::map<std::string, std::string> get_node_inventory()
{
  std::map<std::string, std::string> node_inventory = {
      {"Abs", "Math/Base"},
      {"AbsSmooth", "Math/Base"},
      {"AccumulationCurvature", "WIP/DEPRECATED"},
      {"AreaRemove", "Terrain Features/Morphology"},
      {"Badlands", "Primitive/Geological"},
      {"Band", "Primitive/Function"},
      {"BasaltField", "Primitive/Geological"},
      {"Blend", "Operator/Blend"},
      {"Blend3", "Operator/Blend"},
      {"BlenderBridge", "Bridges"},
      {"BlendPoissonBf", "Operator/Blend"},
      {"Border", "WIP/DEPRECATED"},
      {"Brush", "Primitive/Authoring"},
      {"Broadcast", "Routing"},
      {"Bulkify", "Boundaries"},
      {"Bump", "Primitive/Function"},
      {"BumpLorentzian", "Primitive/Function"},
      {"Caldera", "WIP"}, // "Primitive/Geological"},
      {"Clamp", "Filter/Range"},
      {"ClampOblique", "Filter/Range"},
      {"Closing", "WIP/DEPRECATED"},
      {"Cloud", "Geometry/Cloud"},
      {"CloudFromCsv", "Geometry/Cloud"},
      {"CloudMerge", "Geometry/Cloud"},
      {"CloudLattice", "Geometry/Cloud"},
      {"CloudRandom", "Geometry/Cloud"},
      {"CloudRandomDensity", "Geometry/Cloud"},
      {"CloudRandomDistance", "Geometry/Cloud"},
      {"CloudRandomPowerLaw", "Geometry/Cloud"},
      {"CloudRandomWeibull", "Geometry/Cloud"},
      {"CloudRemapValues", "Geometry/Cloud"},
      {"CloudSDF", "Geometry/Cloud"},
      {"CloudSetValuesFromBorderDistance", "Geometry/Cloud"},
      {"CloudSetValuesFromHeightmap", "Geometry/Cloud"},
      {"CloudSetValuesFromMinDistance", "Geometry/Cloud"},
      {"CloudShuffle", "Geometry/Cloud"},
      {"CloudToArrayInterp", "Geometry/Cloud"},
      {"CloudToPath", "Geometry/Cloud"},
      {"CloudToVectors", "Geometry/Cloud"},
      {"CoastalErosionDiffusion", "Erosion/Water"},
      {"CoastalErosionProfile", "Erosion/Water"},
      {"CoastalFetch", "WIP"}, // "Terrain Features/Advanced"},
      {"CellularNoise", "Primitive/Coherent"},
      {"CoherentNoise", "Primitive/Coherent"},
      {"ColorAdjust", "Texture"},
      {"ColorizeBivariate", "Texture"},
      {"ColorizeGradient", "Texture"},
      {"ColorizeSolid", "Texture"},
      {"CombineMask", "Terrain Features/Mask Operations"},
      {"Compare", "Debug"},
      {"Cone", "Primitive/Function"},
      {"ConeComplex", "Primitive/Function"},
      {"ConeSigmoid", "Primitive/Function"},
      {"Constant", "Primitive/Function"},
      {"ConvolveSVD", "Math/Convolution"},
      {"Cos", "Math/Base"},
      {"Crater", "Primitive/Geological"},
      {"CurvatureMean", "WIP/DEPRECATED"},
      {"Curvatures", "Terrain Features"},
      {"Debug", "Debug"},
      {"Dendry", "WIP"},
      {"DepositionFillHoles", "Erosion/Deposition"},
      {"DepressionFilling", "Erosion/Deposition"},
      {"Detrend", "WIP"},                     // Filter/Recurve
      {"DiffusionLimitedAggregation", "WIP"}, // Primitive/Coherent
      {"Dilation", "WIP/DEPRECATED"},
      {"DirectionalBlur", "Filter/Smoothing"},
      {"DistanceTransform", "Terrain Features/Morphology"},
      {"Equalize", "Filter/Recurve"},
      {"Erosion", "WIP/DEPRECATED"},
      {"ExpandShrink", "Filter/Recast"},
      {"ExportAsset", "Export"},
      {"ExportAsCubemap", "WIP"}, // "Export"},
      {"ExportCloud", "Export"},
      {"ExportCloudToPly", "Export"},
      {"ExportHeightmap", "Export"},
      {"ExportNormalMap", "Export"},
      {"ExportPath", "Export"},
      {"ExportPointsToPly", "Export"},
      {"ExportTexture", "Export"},
      {"ExportTiled", "Export"},
      {"Falloff", "Boundaries"},
      {"FillTalus", "Filter/Advanced Filters"},
      {"FindCutPath", "Geometry/Path"},
      {"FlatbedCarve", "Hydrology"},
      {"FloodingFromBoundaries", "Hydrology"},
      {"FloodingFromPoint", "Hydrology"},
      {"FloodingLakeSystem", "Hydrology"},
      {"FloodingUniformLevel", "Hydrology"},
      {"FlowSimulation", "Hydrology"},
      {"FlowSimulationViscous", "Hydrology"},
      {"FlowStream", "WIP"}, // Hydrology
      {"Fold", "Filter/Recast"},
      {"GaborWaveFbm", "Primitive/Coherent"},
      {"Gain", "Filter/Recurve"},
      {"GammaCorrection", "Filter/Recurve"},
      {"GammaCorrectionLocal", "Filter/Recurve"},
      {"GaussianDecay", "Math/Base"},
      {"GaussianPulse", "Primitive/Function"},
      {"Gavoronoise", "Primitive/Coherent"},
      {"Gradient", "Math/Gradient"},
      {"GradientAngle", "Math/Gradient"},
      {"GradientNorm", "Math/Gradient"},
      {"GradientTalus", "Math/Gradient"},
      {"HeightmapToKernel", "Converter"},
      {"HeightmapToMask", "Converter"},
      {"HeightmapToNormalMap", "Converter"},
      {"HeightmapToRGBA", "Converter"},
      {"HemisphereFieldFbm", "Primitive/Coherent"},
      {"HydraulicBlur", "WIP"},     // "Erosion/Hydraulic"
      {"HydraulicMusgrave", "WIP"}, // "Erosion/Hydraulic"
      {"HydraulicParticle", "Erosion/Hydraulic"},
      {"HydraulicProcedural", "Erosion/Hydraulic"},
      {"HydraulicSaleve", "Erosion/Hydraulic"},
      {"HydraulicSchott", "WIP"}, // "Erosion/Hydraulic"
      {"HydraulicStream", "WIP"}, // "Erosion/Hydraulic"
      {"HydraulicStreamLog", "Erosion/Hydraulic"},
      {"HydraulicStreamUpscaleAmplification", "WIP"}, // "Erosion/Hydraulic"
      {"HydraulicVpipes", "WIP"},                     // "Erosion/Hydraulic"
      {"ImportHeightmap", "Export"},
      {"ImportTexture", "Export"},
      {"Inverse", "Math/Base"},
      {"Island", "Primitive/Geological"},
      {"IslandChain", "Primitive/Geological"},
      {"IslandLandMask", "Primitive/Geological"},
      {"KernelCupola", "Primitive/Kernel"},
      {"KernelDiskSmooth", "Primitive/Kernel"},
      {"KernelGabor", "Primitive/Kernel"},
      {"KernelPrim", "Primitive/Kernel"},
      {"KmeansClustering2", "Terrain Features/Clustering"},
      {"KmeansClustering3", "Terrain Features/Clustering"},
      {"Kuwahara", "WIP"}, // Filter/Smoothing
      {"Laplace", "Filter/Smoothing"},
      {"Lerp", "Math/Base"},
      {"LocalMetrics", "Terrain Features"},
      {"LevelSetCurvature", "WIP"}, // "Terrain Features/Advanced"},
      {"MakeBinary", "Terrain Features/Morphology"},
      {"MakePeriodic", "Operator/Tiling"},
      {"MakePeriodicStitching", "Operator/Tiling"},
      {"MeanShift", "WIP"},
      {"Median3x3", "Filter/Smoothing"},
      {"MedianPseudo", "WIP"},
      {"MergeWaterDepths", "Hydrology"},
      {"MixNormalMap", "Texture"},
      {"MixTexture", "Texture"},
      {"Mixer", "Operator/Blend"},
      {"MorphologicalGradient", "WIP/DEPRECATED"},
      {"MorphologicalOperators", "Terrain Features"},
      {"MorphologicalTopHat", "WIP/DEPRECATED"},
      {"MountainCone", "Primitive/Geological"},
      {"MountainInselberg", "Primitive/Geological"},
      {"MountainRangeRadial", "WIP"}, // , "Primitive/Geological"},
      {"MountainStump", "Primitive/Geological"},
      {"MountainTibesti", "Primitive/Geological"},
      {"Mudslide", "WIP"}, // "Erosion"},
      {"Multisteps", "Primitive/Geological"},
      {"Noise", "WIP/DEPRECATED"},
      {"NoiseFbm", "WIP/DEPRECATED"},
      {"NoiseIq", "WIP/DEPRECATED"},
      {"NoiseJordan", "WIP/DEPRECATED"},
      {"NoiseParberry", "WIP/DEPRECATED"},
      {"NoisePingpong", "WIP/DEPRECATED"},
      {"NoiseRidged", "WIP/DEPRECATED"},
      {"NoiseSwiss", "WIP/DEPRECATED"},
      {"NormalDisplacement", "WIP"},
      {"NormalMapToHeightmap", "WIP"},
      {"Opening", "WIP/DEPRECATED"},
      {"Paraboloid", "Primitive/Function"},
      {"Path", "Geometry/Path"},
      {"PathBezier", "WIP/DEPRECATED"},
      {"PathBezierRound", "WIP/DEPRECATED"},
      {"PathBspline", "WIP/DEPRECATED"},
      {"PathDecasteljau", "WIP/DEPRECATED"},
      {"PathDecimate", "Geometry/Path"},
      {"PathDig", "Geometry/Path"},
      {"PathFind", "Geometry/Path"},
      {"PathFractalize", "Geometry/Path"},
      {"PathFromCsv", "Geometry/Path"},
      {"PathInflate", "Geometry/Path"},
      {"PathMeanderize", "Geometry/Path"},
      {"PathResample", "Geometry/Path"},
      {"PathSDF", "Geometry/Path"},
      {"PathShuffle", "Geometry/Path"},
      {"PathSmooth", "Geometry/Path"},
      {"PathToCloud", "Geometry/Path"},
      {"PathToHeightmap", "Geometry/Path"},
      {"Phasor", "Primitive/Coherent"},
      {"Plateau", "Filter/Recurve"},
      {"Plates", "Primitive/Geological"},
      {"PolarShape", "Primitive/Function"},
      {"PolygonField", "Primitive/Coherent"},
      {"PolygonFieldFbm", "Primitive/Coherent"},
      {"PostProcess", "Filter"},
      {"Preview", "Debug"},
      {"ProjectTalus", "Filter"},
      {"QuiltingBlend", "Operator/Resynthesis"},
      {"QuiltingExpand", "Operator/Resynthesis"},
      {"QuiltingShuffle", "Operator/Resynthesis"},
      {"RadialDisplacementToXy", "Math"},
      {"RecastCanyon", "WIP"},           // "Filter/Recast"},
      {"RecastCliff", "WIP"},            // "Filter/Recast"},
      {"RecastCliffDirectional", "WIP"}, // "Filter/Recast"},
      {"RecastCracks", "WIP"},           // "Filter/Recast"},
      {"RecastSag", "WIP"},              // "Filter/Recast"},
      {"Receive", "Routing"},
      {"Recurve", "Filter/Recurve"},
      {"RecurveKura", "Filter/Recurve"},
      {"RecurveS", "WIP"},
      {"RelativeDistanceFromSkeleton", "Terrain Features/Morphology"},
      {"RelativeElevation", "WIP/DEPRECATED"},
      {"Remap", "Filter/Range"},
      {"Rescale", "Filter/Range"},
      {"ReverseAboveThreshold", "Filter/Recurve"},
      {"ReverseMidpoint", "WIP"}, // Primitive/Authoring
      {"Ridgelines", "Primitive/Authoring"},
      {"Rift", "Primitive/Geological"},
      {"Rifts", "Erosion/Hydraulic"},
      {"Rotate", "Operator/Transform"},
      {"RotateDisplacement", "Math"},
      {"Ruggedness", "WIP/DEPRECATED"},
      {"Rugosity", "Terrain Features/Advanced"},
      {"Saturate", "Filter/Recurve"},
      {"ScanMask", "Terrain Features/Mask Operations"},
      {"SedimentDeposition", "WIP"}, // "Erosion/Deposition"
      {"SelectAngle", "Terrain Features/Selector"},
      {"SelectBlobLog", "Terrain Features/Selector"},
      {"SelectCavities", "Terrain Features/Selector"},
      {"SelectInterval", "Terrain Features/Selector"},
      {"SelectInwardOutward", "Terrain Features/Selector"},
      {"SelectMidrange", "Terrain Features/Selector"},
      {"SelectMultiband3", "Terrain Features/Selector"},
      {"SelectRivers", "Terrain Features/Selector"},
      {"SelectSlope", "Terrain Features/Selector"},
      {"SelectSoilFlow", "Terrain Features/ForTexturing"},
      {"SelectSoilRocks", "Terrain Features/ForTexturing"},
      {"SelectSoilWeathered", "Terrain Features/ForTexturing"},
      {"SelectThreshold", "Terrain Features/Selector"},
      {"SelectTransitions", "Terrain Features/Selector"},
      {"SelectValley", "Terrain Features/Selector"},
      {"SelectValue", "Terrain Features/Selector"},
      {"SetAlpha", "Texture"},
      {"SetBorders", "Boundaries"},
      {"ShapeIndex", "WIP/DEPRECATED"},
      {"SharpenCone", "WIP"},
      {"ShatteredPeak", "Primitive/Geological"},
      {"ShiftElevation", "Filter/Range"},
      {"Skeleton", "Terrain Features/Morphology"},
      {"Slope", "Primitive/Function"},
      {"SmoothCpulse", "Filter/Smoothing"},
      {"SmoothFill", "Filter/Smoothing"},
      {"SmoothFillHoles", "Filter/Smoothing"},
      {"SmoothFillSmearPeaks", "Filter/Smoothing"},
      {"Smoothstep", "Math/Base"},
      {"SnowMeltingMap", "Hydrology"},
      {"SnowSimulation", "Hydrology"},
      {"SpectralEqualizer", "Filter"},
      {"Stamping", "Primitive/Authoring"},
      {"SteepenConvective", "WIP"},
      {"Step", "Primitive/Function"},
      {"Strata", "Erosion/Stratify"},
      {"StrataCells", "Erosion/Stratify"},
      {"StrataPlates", "Erosion/Stratify"},
      {"StrataTerrace", "Erosion/Stratify"},
      {"Terrace", "WIP/DEPRECATED"},
      {"TextureAdvectionParticle", "Texture"},
      {"TextureAdvectionWarp", "WIP/DEPRECATED"},
      {"TextureQuiltingExpand", "Texture"},
      {"TextureQuiltingShuffle", "Texture"},
      {"TextureSelectColor", "Texture"},
      {"TextureSplitChannels", "Texture"},
      {"TextureToHeightmap", "Texture"},
      {"TextureUvChecker", "Texture"},
      {"TextureTransform", "Texture"},
      {"Thermal", "Erosion/Deposition"},
      {"ThermalFlatten", "Erosion/Deposition"},
      {"ThermalScree", "Erosion/Deposition"},
      {"Thru", "Routing"},
      {"Toggle", "Routing"},
      {"Transfer", "Operator/Blend"},
      {"Translate", "Operator/Transform"},
      {"Unsphericity", "WIP/DEPRECATED"},
      {"ValleyFill", "Erosion/Deposition"},
      {"ValleyWidth", "Terrain Features/Landform"},
      {"Vorolines", "WIP/DEPRECATED"},
      {"VorolinesFbm", "WIP/DEPRECATED"},
      {"Voronoi", "WIP/DEPRECATED"},
      {"VoronoiFbm", "WIP/DEPRECATED"},
      {"Voronoise", "WIP/DEPRECATED"},
      {"Vororand", "WIP/DEPRECATED"},
      {"Warp", "Operator/Transform"},
      {"WarpDownslope", "WIP"}, // Operator/Transform
      {"WaterDepthDryOut", "Hydrology"},
      {"WaterDepthFromMask", "Hydrology"},
      {"WaterElevationFromDepth", "Hydrology"},
      {"WaterMask", "Hydrology"},
      {"WatershedRidge", "WIP"}, // , "Erosion/Hydraulic"},
      {"WaveDune", "Primitive/Function"},
      {"WaveletNoise", "Primitive/Coherent"},
      {"WaveSine", "Primitive/Function"},
      {"WaveSquare", "Primitive/Function"},
      {"WaveTriangular", "Primitive/Function"},
      {"White", "Primitive/Random"},
      {"WhiteDensityMap", "Primitive/Random"},
      {"WhiteSparse", "Primitive/Random"},
      {"ZeroedEdges", "Boundaries"},
      {"Zoom", "Operator/Transform"},
  };

  return node_inventory;
} // namespace hesiod

std::shared_ptr<gnode::Node> node_factory(const std::string         &node_type,
                                          std::weak_ptr<GraphConfig> config)
{
  // --- specialized nodes

  if (node_type == "Broadcast")
  {
    auto sptr = std::make_shared<hesiod::BroadcastNode>(node_type, config);
    setup_broadcast_node(*sptr);
    sptr->set_compute_fct(&compute_broadcast_node);
    sptr->finalize_attributes();
    return sptr;
  }
  else if (node_type == "Receive")
  {
    auto sptr = std::make_shared<hesiod::ReceiveNode>(node_type, config);
    setup_receive_node(*sptr);
    sptr->set_compute_fct(&compute_receive_node);
    sptr->finalize_attributes();
    return sptr;
  }

  // --- generic nodes

  auto sptr = std::make_shared<hesiod::BaseNode>(node_type, config);

  switch (str2int(node_type.c_str()))
  {
    SETUP_NODE(BlenderBridge, blender_bridge);
    SETUP_NODE(Debug, debug);
    SETUP_NODE(ExportHeightmap, export_heightmap);
    SETUP_NODE(FloodingUniformLevel, flooding_uniform_level);
    SETUP_NODE(GaborWaveFbm, gabor_wave_fbm);
    SETUP_NODE(Gain, gain);
    SETUP_NODE(HydraulicStreamLog, hydraulic_stream_log);
    SETUP_NODE(Noise, noise);
    SETUP_NODE(Preview, preview);
    SETUP_NODE(SmoothCpulse, smooth_cpulse);
    SETUP_NODE(Toggle, toggle);
    SETUP_NODE(Thru, thru);

#ifndef HESIOD_MINIMAL_NODE_SET
    SETUP_NODE(Abs, abs);
    SETUP_NODE(AbsSmooth, abs_smooth);
    SETUP_NODE(AccumulationCurvature, accumulation_curvature);
    SETUP_NODE(AreaRemove, area_remove);
    SETUP_NODE(Badlands, badlands);
    SETUP_NODE(Band, band);
    SETUP_NODE(BasaltField, basalt_field);
    SETUP_NODE(Blend, blend);
    SETUP_NODE(Blend3, blend3);
    SETUP_NODE(BlendPoissonBf, blend_poisson_bf);
    SETUP_NODE(Border, border);
    SETUP_NODE(Brush, brush);
    SETUP_NODE(Bulkify, bulkify);
    SETUP_NODE(Bump, bump);
    SETUP_NODE(BumpLorentzian, bump_lorentzian);
    SETUP_NODE(Caldera, caldera);
    SETUP_NODE(Clamp, clamp);
    SETUP_NODE(ClampOblique, clamp_oblique);
    SETUP_NODE(Closing, closing);
    SETUP_NODE(CellularNoise, cellular_noise);
    SETUP_NODE(Cloud, cloud);
    SETUP_NODE(CloudFromCsv, cloud_from_csv);
    SETUP_NODE(CloudMerge, cloud_merge);
    SETUP_NODE(CloudLattice, cloud_lattice);
    SETUP_NODE(CloudRandom, cloud_random);
    SETUP_NODE(CloudRandomDensity, cloud_random_density);
    SETUP_NODE(CloudRandomDistance, cloud_random_distance);
    SETUP_NODE(CloudRandomPowerLaw, cloud_random_power_law);
    SETUP_NODE(CloudRandomWeibull, cloud_random_weibull);
    SETUP_NODE(CloudRemapValues, cloud_remap_values);
    SETUP_NODE(CloudSDF, cloud_sdf);
    SETUP_NODE(CloudSetValuesFromBorderDistance, cloud_set_values_from_border_distance);
    SETUP_NODE(CloudSetValuesFromHeightmap, cloud_set_values_from_heightmap);
    SETUP_NODE(CloudSetValuesFromMinDistance, cloud_set_values_from_min_distance);
    SETUP_NODE(CloudShuffle, cloud_shuffle);
    SETUP_NODE(CloudToArrayInterp, cloud_to_array_interp);
    SETUP_NODE(CloudToPath, cloud_to_path);
    SETUP_NODE(CloudToVectors, cloud_to_vectors);
    SETUP_NODE(CoastalErosionDiffusion, coastal_erosion_diffusion);
    SETUP_NODE(CoastalErosionProfile, coastal_erosion_profile);
    SETUP_NODE(CoastalFetch, coastal_fetch);
    SETUP_NODE(CoherentNoise, coherent_noise);
    SETUP_NODE(ColorAdjust, color_adjust)
    SETUP_NODE(ColorizeBivariate, colorize_bivariate);
    SETUP_NODE(ColorizeGradient, colorize_gradient);
    SETUP_NODE(ColorizeSolid, colorize_solid);
    SETUP_NODE(CombineMask, combine_mask);
    SETUP_NODE(Compare, compare);
    SETUP_NODE(Cone, cone);
    SETUP_NODE(ConeComplex, cone_complex);
    SETUP_NODE(ConeSigmoid, cone_sigmoid);
    SETUP_NODE(Constant, constant);
    SETUP_NODE(Cos, cos);
    SETUP_NODE(ConvolveSVD, convolve_svd);
    SETUP_NODE(Crater, crater);
    SETUP_NODE(CurvatureMean, curvature_mean);
    SETUP_NODE(Curvatures, curvatures);
    SETUP_NODE(Dendry, dendry);
    SETUP_NODE(DepositionFillHoles, deposition_fill_holes);
    SETUP_NODE(DepressionFilling, depression_filling);
    SETUP_NODE(Detrend, detrend);
    SETUP_NODE(DiffusionLimitedAggregation, diffusion_limited_aggregation);
    SETUP_NODE(Dilation, dilation);
    SETUP_NODE(DirectionalBlur, directional_blur);
    SETUP_NODE(DistanceTransform, distance_transform);
    SETUP_NODE(Equalize, equalize);
    SETUP_NODE(Erosion, erosion);
    SETUP_NODE(ExpandShrink, expand_shrink);
    SETUP_NODE(ExportAsset, export_asset);
    SETUP_NODE(ExportAsCubemap, export_as_cubemap);
    SETUP_NODE(ExportCloud, export_cloud);
    SETUP_NODE(ExportCloudToPly, export_cloud_to_ply);
    SETUP_NODE(ExportNormalMap, export_normal_map);
    SETUP_NODE(ExportPath, export_path);
    SETUP_NODE(ExportPointsToPly, export_points_to_ply);
    SETUP_NODE(ExportTexture, export_texture);
    SETUP_NODE(ExportTiled, export_tiled);
    SETUP_NODE(Falloff, falloff);
    SETUP_NODE(FillTalus, fill_talus);
    SETUP_NODE(FindCutPath, find_cut_path);
    SETUP_NODE(FlatbedCarve, flatbed_carve);
    SETUP_NODE(FloodingFromBoundaries, flooding_from_boundaries);
    SETUP_NODE(FloodingFromPoint, flooding_from_point);
    SETUP_NODE(FloodingLakeSystem, flooding_lake_system);
    SETUP_NODE(FlowSimulation, flow_simulation);
    SETUP_NODE(FlowSimulationViscous, flow_simulation_viscous);
    SETUP_NODE(FlowStream, flow_stream);
    SETUP_NODE(Fold, fold);
    SETUP_NODE(GammaCorrection, gamma_correction);
    SETUP_NODE(GammaCorrectionLocal, gamma_correction_local);
    SETUP_NODE(GaussianDecay, gaussian_decay);
    SETUP_NODE(GaussianPulse, gaussian_pulse);
    SETUP_NODE(Gavoronoise, gavoronoise);
    SETUP_NODE(Gradient, gradient);
    SETUP_NODE(GradientAngle, gradient_angle);
    SETUP_NODE(GradientNorm, gradient_norm);
    SETUP_NODE(GradientTalus, gradient_talus);
    SETUP_NODE(HeightmapToKernel, heightmap_to_kernel);
    SETUP_NODE(HeightmapToMask, heightmap_to_mask);
    SETUP_NODE(HeightmapToNormalMap, heightmap_to_normal_map);
    SETUP_NODE(HeightmapToRGBA, heightmap_to_rgba);
    SETUP_NODE(HemisphereFieldFbm, hemisphere_field_fbm);
    SETUP_NODE(HydraulicBlur, hydraulic_blur);
    SETUP_NODE(HydraulicMusgrave, hydraulic_musgrave);
    SETUP_NODE(HydraulicParticle, hydraulic_particle);
    SETUP_NODE(HydraulicProcedural, hydraulic_procedural);
    SETUP_NODE(HydraulicSaleve, hydraulic_saleve);
    SETUP_NODE(HydraulicSchott, hydraulic_schott);
    SETUP_NODE(HydraulicStream, hydraulic_stream);
    SETUP_NODE(HydraulicStreamUpscaleAmplification,
               hydraulic_stream_upscale_amplification);
    SETUP_NODE(HydraulicVpipes, hydraulic_vpipes);
    SETUP_NODE(ImportHeightmap, import_heightmap);
    SETUP_NODE(ImportTexture, import_texture);
    SETUP_NODE(Inverse, inverse);
    SETUP_NODE(Island, island);
    SETUP_NODE(IslandChain, island_chain);
    SETUP_NODE(IslandLandMask, island_land_mask);
    SETUP_NODE(KernelCupola, kernel_cupola);
    SETUP_NODE(KernelDiskSmooth, kernel_disk_smooth);
    SETUP_NODE(KernelGabor, kernel_gabor);
    SETUP_NODE(KernelPrim, kernel_prim);
    SETUP_NODE(KmeansClustering2, kmeans_clustering2);
    SETUP_NODE(KmeansClustering3, kmeans_clustering3);
    SETUP_NODE(Kuwahara, kuwahara);
    SETUP_NODE(Laplace, laplace);
    SETUP_NODE(Lerp, lerp);
    SETUP_NODE(LevelSetCurvature, level_set_curvature);
    SETUP_NODE(LocalMetrics, local_metrics);
    SETUP_NODE(MakeBinary, make_binary);
    SETUP_NODE(MakePeriodic, make_periodic);
    SETUP_NODE(MakePeriodicStitching, make_periodic_stitching);
    SETUP_NODE(MeanShift, mean_shift);
    SETUP_NODE(Median3x3, median3x3);
    SETUP_NODE(MedianPseudo, median_pseudo);
    SETUP_NODE(MergeWaterDepths, merge_water_depths);
    SETUP_NODE(Mixer, mixer);
    SETUP_NODE(MixNormalMap, mix_normal_map);
    SETUP_NODE(MixTexture, mix_texture);
    SETUP_NODE(MorphologicalGradient, morphological_gradient);
    SETUP_NODE(MorphologicalOperators, morphological_operators);
    SETUP_NODE(MorphologicalTopHat, morphological_top_hat);
    SETUP_NODE(MountainCone, mountain_cone);
    SETUP_NODE(MountainInselberg, mountain_inselberg);
    SETUP_NODE(MountainRangeRadial, mountain_range_radial);
    SETUP_NODE(MountainStump, mountain_stump);
    SETUP_NODE(MountainTibesti, mountain_tibesti);
    SETUP_NODE(Mudslide, mudslide);
    SETUP_NODE(Multisteps, multisteps);
    SETUP_NODE(NoiseFbm, noise_fbm);
    SETUP_NODE(NoiseIq, noise_iq);
    SETUP_NODE(NoiseJordan, noise_iq);
    SETUP_NODE(NoiseParberry, noise_parberry);
    SETUP_NODE(NoisePingpong, noise_pingpong);
    SETUP_NODE(NoiseRidged, noise_ridged);
    SETUP_NODE(NoiseSwiss, noise_swiss);
    SETUP_NODE(NormalDisplacement, normal_displacement);
    SETUP_NODE(NormalMapToHeightmap, normal_map_to_heightmap);
    SETUP_NODE(Opening, opening);
    SETUP_NODE(Paraboloid, paraboloid);
    SETUP_NODE(Path, path);
    SETUP_NODE(PathBezier, path_bezier);
    SETUP_NODE(PathBezierRound, path_bezier_round);
    SETUP_NODE(PathBspline, path_bspline);
    SETUP_NODE(PathDecasteljau, path_decasteljau);
    SETUP_NODE(PathDecimate, path_decimate);
    SETUP_NODE(PathDig, path_dig);
    SETUP_NODE(PathFind, path_find);
    SETUP_NODE(PathFractalize, path_fractalize);
    SETUP_NODE(PathFromCsv, path_from_csv);
    SETUP_NODE(PathInflate, path_inflate);
    SETUP_NODE(PathMeanderize, path_meanderize);
    SETUP_NODE(PathResample, path_resample);
    SETUP_NODE(PathSDF, path_sdf);
    SETUP_NODE(PathShuffle, path_shuffle);
    SETUP_NODE(PathSmooth, path_smooth);
    SETUP_NODE(PathToCloud, path_to_cloud);
    SETUP_NODE(PathToHeightmap, path_to_heightmap);
    SETUP_NODE(Phasor, phasor);
    SETUP_NODE(Plateau, plateau);
    SETUP_NODE(Plates, plates);
    SETUP_NODE(PolarShape, polar_shape);
    SETUP_NODE(PolygonField, polygon_field);
    SETUP_NODE(PolygonFieldFbm, polygon_field_fbm);
    SETUP_NODE(PostProcess, post_process);
    SETUP_NODE(ProjectTalus, project_talus);
    SETUP_NODE(QuiltingBlend, quilting_blend);
    SETUP_NODE(QuiltingExpand, quilting_expand);
    SETUP_NODE(QuiltingShuffle, quilting_shuffle);
    SETUP_NODE(RadialDisplacementToXy, radial_displacement_to_xy);
    SETUP_NODE(RecastCanyon, recast_canyon);
    SETUP_NODE(RecastCliff, recast_cliff);
    SETUP_NODE(RecastCliffDirectional, recast_cliff_directional);
    SETUP_NODE(RecastCracks, recast_cracks);
    SETUP_NODE(RecastSag, recast_sag);
    SETUP_NODE(Recurve, recurve);
    SETUP_NODE(RecurveKura, recurve_kura);
    SETUP_NODE(RecurveS, recurve_s);
    SETUP_NODE(RelativeDistanceFromSkeleton, relative_distance_from_skeleton);
    SETUP_NODE(RelativeElevation, relative_elevation);
    SETUP_NODE(Remap, remap);
    SETUP_NODE(Rescale, rescale);
    SETUP_NODE(ReverseAboveThreshold, reverse_above_theshold);
    SETUP_NODE(ReverseMidpoint, reverse_midpoint);
    SETUP_NODE(Ridgelines, ridgelines);
    SETUP_NODE(Rift, rift);
    SETUP_NODE(Rifts, rifts);
    SETUP_NODE(Rotate, rotate);
    SETUP_NODE(RotateDisplacement, rotate_displacement);
    SETUP_NODE(Ruggedness, ruggedness);
    SETUP_NODE(Rugosity, rugosity);
    SETUP_NODE(Saturate, saturate);
    SETUP_NODE(ScanMask, scan_mask);
    SETUP_NODE(SedimentDeposition, sediment_deposition);
    SETUP_NODE(SelectAngle, select_angle);
    SETUP_NODE(SelectBlobLog, select_blob_log);
    SETUP_NODE(SelectCavities, select_cavities);
    SETUP_NODE(SelectInterval, select_interval);
    SETUP_NODE(SelectInwardOutward, select_inward_outward);
    SETUP_NODE(SelectMultiband3, select_multiband3);
    SETUP_NODE(SelectMidrange, select_midrange);
    SETUP_NODE(SelectRivers, select_rivers);
    SETUP_NODE(SelectSlope, select_slope);
    SETUP_NODE(SelectSoilFlow, select_soil_flow);
    SETUP_NODE(SelectSoilRocks, select_soil_rocks);
    SETUP_NODE(SelectSoilWeathered, select_soil_weathered);
    SETUP_NODE(SelectThreshold, select_threshold);
    SETUP_NODE(SelectTransitions, select_transitions);
    SETUP_NODE(SelectValley, select_valley);
    SETUP_NODE(SelectValue, select_value);
    SETUP_NODE(SetAlpha, set_alpha);
    SETUP_NODE(SetBorders, set_borders);
    SETUP_NODE(ShapeIndex, shape_index);
    SETUP_NODE(SharpenCone, sharpen_cone);
    SETUP_NODE(ShatteredPeak, shattered_peak);
    SETUP_NODE(ShiftElevation, shift_elevation);
    SETUP_NODE(Skeleton, skeleton);
    SETUP_NODE(Slope, slope);
    SETUP_NODE(SmoothFill, smooth_fill);
    SETUP_NODE(SmoothFillHoles, smooth_fill_holes);
    SETUP_NODE(SmoothFillSmearPeaks, smooth_fill_smear_peaks);
    SETUP_NODE(Smoothstep, smoothstep);
    SETUP_NODE(SnowMeltingMap, snow_melting_map);
    SETUP_NODE(SnowSimulation, snow_simulation);
    SETUP_NODE(SpectralEqualizer, spectral_equalizer);
    SETUP_NODE(Stamping, stamping);
    SETUP_NODE(SteepenConvective, steepen_convective);
    SETUP_NODE(Step, step);
    SETUP_NODE(Strata, strata);
    SETUP_NODE(StrataCells, strata_cells);
    SETUP_NODE(StrataPlates, strata_plates);
    SETUP_NODE(StrataTerrace, strata_terrace);
    SETUP_NODE(Terrace, terrace);
    SETUP_NODE(TextureAdvectionParticle, texture_advection_particle);
    SETUP_NODE(TextureAdvectionWarp, texture_advection_warp);
    SETUP_NODE(TextureQuiltingExpand, texture_quilting_expand);
    SETUP_NODE(TextureQuiltingShuffle, texture_quilting_shuffle);
    SETUP_NODE(TextureSelectColor, texture_select_color);
    SETUP_NODE(TextureSplitChannels, texture_split_channels);
    SETUP_NODE(TextureToHeightmap, texture_to_heightmap);
    SETUP_NODE(TextureUvChecker, texture_uv_checker);
    SETUP_NODE(TextureTransform, texture_transform);
    SETUP_NODE(Transfer, transfer);
    SETUP_NODE(Translate, translate);
    SETUP_NODE(Thermal, thermal);
    SETUP_NODE(ThermalFlatten, thermal_flatten);
    SETUP_NODE(ThermalScree, thermal_scree);
    SETUP_NODE(Unsphericity, unsphericity);
    SETUP_NODE(ValleyFill, valley_fill);
    SETUP_NODE(ValleyWidth, valley_width);
    SETUP_NODE(Vorolines, vorolines);
    SETUP_NODE(VorolinesFbm, vorolines_fbm);
    SETUP_NODE(Voronoi, voronoi);
    SETUP_NODE(VoronoiFbm, voronoi_fbm);
    SETUP_NODE(Voronoise, voronoise);
    SETUP_NODE(Vororand, vororand);
    SETUP_NODE(Warp, warp);
    SETUP_NODE(WarpDownslope, warp_downslope);
    SETUP_NODE(WaterDepthDryOut, water_depth_dry_out);
    SETUP_NODE(WaterDepthFromMask, water_depth_from_mask);
    SETUP_NODE(WaterElevationFromDepth, water_elevation_from_depth);
    SETUP_NODE(WaterMask, water_mask);
    SETUP_NODE(WatershedRidge, watershed_ridge);
    SETUP_NODE(WaveDune, wave_dune);
    SETUP_NODE(WaveletNoise, wavelet_noise);
    SETUP_NODE(WaveSine, wave_sine);
    SETUP_NODE(WaveSquare, wave_square);
    SETUP_NODE(WaveTriangular, wave_triangular);
    SETUP_NODE(White, white);
    SETUP_NODE(WhiteDensityMap, white_density_map);
    SETUP_NODE(WhiteSparse, white_sparse);
    SETUP_NODE(ZeroedEdges, zeroed_edges);
    SETUP_NODE(Zoom, zoom);
#endif
  default:
    throw std::invalid_argument("Unknown node type in node_factory: " + node_type);
  }

  return sptr;
}

} // namespace hesiod
