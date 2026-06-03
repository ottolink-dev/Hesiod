/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once

#include "highmap/authoring.hpp"
#include "highmap/boundary.hpp"
#include "highmap/colormaps.hpp"
#include "highmap/curvature.hpp"
#include "highmap/erosion.hpp"
#include "highmap/kernels.hpp"
#include "highmap/local_metrics.hpp"
#include "highmap/math.hpp"
#include "highmap/morphology.hpp"
#include "highmap/primitives.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

static struct EnumMappings
{

  const std::map<std::string, int> blending_method_map = {
      {"add", BlendingMethod::ADD},
      {"exclusion", BlendingMethod::EXCLUSION_BLEND},
      {"gradients", BlendingMethod::GRADIENTS},
      {"maximum", BlendingMethod::MAXIMUM},
      {"maximum_smooth", BlendingMethod::MAXIMUM_SMOOTH},
      {"minimum", BlendingMethod::MINIMUM},
      {"minimum_smooth", BlendingMethod::MINIMUM_SMOOTH},
      {"multiply", BlendingMethod::MULTIPLY},
      {"multiply_add", BlendingMethod::MULTIPLY_ADD},
      {"negate", BlendingMethod::NEGATE},
      {"overlay", BlendingMethod::OVERLAY},
      {"replace", BlendingMethod::REPLACE},
      {"soft", BlendingMethod::SOFT},
      {"substract", BlendingMethod::SUBSTRACT}};

  const std::map<std::string, int> domain_boundary_map = {
      {"West", hmap::DomainBoundary::BOUNDARY_LEFT},
      {"East", hmap::DomainBoundary::BOUNDARY_RIGHT},
      {"North", hmap::DomainBoundary::BOUNDARY_TOP},
      {"South", hmap::DomainBoundary::BOUNDARY_BOTTOM},
  };

  const std::map<std::string, int> clamping_mode_map = {
      {"Keep positive & clamp", hmap::ClampMode::POSITIVE_ONLY},
      {"Keep negative & clamp", hmap::ClampMode::NEGATIVE_ONLY},
      {"Clamp both", hmap::ClampMode::BOTH},
      {"No clamping", hmap::ClampMode::NONE}};

  const std::map<std::string, int> cmap_map = {
      {"bone", hmap::Cmap::BONE},
      {"gray", hmap::Cmap::GRAY},
      {"jet", hmap::Cmap::JET},
      {"magma", hmap::Cmap::MAGMA},
      {"nipy_spectral", hmap::Cmap::NIPY_SPECTRAL},
      {"terrain", hmap::Cmap::TERRAIN},
      {"viridis", hmap::Cmap::VIRIDIS}};

  const std::map<std::string, int> curvature_type_map = {
      {"Min", hmap::CurvatureType::CT_MIN},
      {"Max", hmap::CurvatureType::CT_MAX},
      {"Mean", hmap::CurvatureType::CT_MEAN},
      {"Gaussian", hmap::CurvatureType::CT_GAUSSIAN},
      {"Profile", hmap::CurvatureType::CT_PROFILE},
      {"Contour", hmap::CurvatureType::CT_CONTOUR},
      {"Tangential", hmap::CurvatureType::CT_TANGENTIAL},
      {"Accumulation", hmap::CurvatureType::CT_ACCUMULATION},
      {"Shape Index", hmap::CurvatureType::CT_SHAPE_INDEX},
      {"Unsphericity", hmap::CurvatureType::CT_UNSPHERICITY},
      {"Ring", hmap::CurvatureType::CT_RING},
      {"Rotor", hmap::CurvatureType::CT_ROTOR}};

  const std::map<std::string, int> distance_function_map = {
      {"Chebyshev", hmap::DistanceFunction::CHEBYSHEV},
      {"Euclidian", hmap::DistanceFunction::EUCLIDIAN},
      {"Euclidian/Chebyshev", hmap::DistanceFunction::EUCLISHEV},
      {"Manhattan", hmap::DistanceFunction::MANHATTAN},
  };

  const std::map<std::string, int> distance_transform_type_map = {
      {"Exact", hmap::DistanceTransformType::DT_EXACT},
      {"Approx. (fast)", hmap::DistanceTransformType::DT_APPROX},
      {"Manhattan (fast)", hmap::DistanceTransformType::DT_MANHATTAN},
      {"Jump flooding (GPU)", hmap::DistanceTransformType::DT_JFA},
  };

  const std::map<std::string, int> erosion_profile_map = {
      {"Cosine", hmap::ErosionProfile::EP_COSINE},
      {"Cosine Bulk", hmap::ErosionProfile::EP_COSINE_BULK},
      {"Cosine Peak", hmap::ErosionProfile::EP_COSINE_PEAK},
      {"Parabol", hmap::ErosionProfile::EP_PARABOL},
      {"Saw Sharp", hmap::ErosionProfile::EP_SAW_SHARP},
      {"Saw Smooth", hmap::ErosionProfile::EP_SAW_SMOOTH},
      {"Sharp Valleys", hmap::ErosionProfile::EP_SHARP_VALLEYS},
      {"Square-root", hmap::ErosionProfile::EP_SQRT},
      {"Triangle Grenier", hmap::ErosionProfile::EP_TRIANGLE_GRENIER},
      {"Triangle Sharp", hmap::ErosionProfile::EP_TRIANGLE_SHARP},
      {"Triangle Smooth", hmap::ErosionProfile::EP_TRIANGLE_SMOOTH}};

  const std::map<std::string, int> flow_direction_method_map = {
      {"D8", hmap::FlowDirectionMethod::FDM_D8},
      {"Priority Flood", hmap::FlowDirectionMethod::FDM_PRIORITY_FLOOD}};

  const std::map<std::string, int> heightmap_export_format_map = {
      {"png (8 bit)", ExportFormat::PNG8BIT},
      {"png (16 bit)", ExportFormat::PNG16BIT},
      {"exr (32 bit), Preserve absolute values", ExportFormat::EXR32BIT},
      {"raw (16 bit, Unity)", ExportFormat::RAW16BIT}};

  const std::map<std::string, int> interpolation_method2d_map = {
      {"Delaunay", hmap::InterpolationMethod2D::ITP2D_DELAUNAY},
      {"Gaussian Weighting", hmap::InterpolationMethod2D::ITP2D_GAUSSIAN},
      {"Inverse Weighting Distance", hmap::InterpolationMethod2D::ITP2D_IDW},
      {"Nearest", hmap::InterpolationMethod2D::ITP2D_NEAREST}};

  const std::map<std::string, int> kernel_type_map = {
      {"biweight", hmap::KernelType::BIWEIGHT},
      {"cone", hmap::KernelType::CONE},
      {"cone_smooth", hmap::KernelType::CONE_SMOOTH},
      {"cubic_pulse", hmap::KernelType::CUBIC_PULSE},
      {"cupola", hmap::KernelType::CUPOLA},
      {"disk", hmap::KernelType::DISK},
      {"lorentzian", hmap::KernelType::LORENTZIAN},
      {"smooth_cosine", hmap::KernelType::SMOOTH_COSINE},
      {"square", hmap::KernelType::SQUARE},
      {"tricube", hmap::KernelType::TRICUBE}};

  const std::map<std::string, int> local_metrics_map = {
      {"Aspect Variance", hmap::gpu::LocalMetrics::LM_LOCAL_ASPECT_VARIANCE},
      {"Maximum", hmap::gpu::LocalMetrics::LM_LOCAL_MAX},
      {"Median Deviation", hmap::gpu::LocalMetrics::LM_LOCAL_MEDIAN_DEVIATION},
      {"Minimum", hmap::gpu::LocalMetrics::LM_LOCAL_MIN},
      {"Relief", hmap::gpu::LocalMetrics::LM_LOCAL_RELIEF},
      {"Variance", hmap::gpu::LocalMetrics::LM_LOCAL_VARIANCE},
      {"Mean", hmap::gpu::LocalMetrics::LM_LOCAL_MEAN},
      // {"Skewness", hmap::gpu::LocalMetrics::LM_LOCAL_SKEWNESS},
      {"Z-Score", hmap::gpu::LocalMetrics::LM_LOCAL_Z_SCORE},
      {"Topographic Position Index",
       hmap::gpu::LocalMetrics::LM_TOPOGRAPHIC_POSITION_INDEX},
      {"Relative Elevation", hmap::gpu::LocalMetrics::LM_RELATIVE_ELEVATION},
      {"Ruggedness", hmap::gpu::LocalMetrics::LM_RUGGEDNESS},
      {"Rugosity Concave", hmap::gpu::LocalMetrics::LM_RUGOSITY_CONCAVE},
      {"Rugosity Convex", hmap::gpu::LocalMetrics::LM_RUGOSITY_CONVEX}};

  const std::map<std::string, int> mask_combine_method_map = {
      {"union", MaskCombineMethod::UNION},
      {"intersection", MaskCombineMethod::INTERSECTION},
      {"exclusion", MaskCombineMethod::EXCLUSION}};

  const std::map<std::string, int> morphology_operation_map = {
      {"Border", hmap::MorphologyOperation::MO_BORDER},
      {"Closing", hmap::MorphologyOperation::MO_CLOSING},
      {"Dilation", hmap::MorphologyOperation::MO_DILATION},
      {"Erosion", hmap::MorphologyOperation::MO_EROSION},
      {"Opening", hmap::MorphologyOperation::MO_OPENING},
      {"Black Hat", hmap::MorphologyOperation::MO_BLACK_HAT},
      {"Top Hat", hmap::MorphologyOperation::MO_TOP_HAT},
      {"Gradient", hmap::MorphologyOperation::MO_GRADIENT},
      {"Laplacian", hmap::MorphologyOperation::MO_LAPLACIAN},
      {"Closing by Reconst.", hmap::MorphologyOperation::MO_CLOSING_BY_RECONSTRUCTION},
      {"Opening by Reconst.", hmap::MorphologyOperation::MO_OPENING_BY_RECONSTRUCTION}};

  const std::map<std::string, int> noise_type_map = {
      {"Perlin", hmap::NoiseType::PERLIN},
      {"Perlin (billow)", hmap::NoiseType::PERLIN_BILLOW},
      {"Perlin (half)", hmap::NoiseType::PERLIN_HALF},
      {"OpenSimplex2", hmap::NoiseType::SIMPLEX2},
      // {"OpenSimplex2S", hmap::NoiseType::SIMPLEX2S},
      {"Value", hmap::NoiseType::VALUE},
      {"Value (cubic)", hmap::NoiseType::VALUE_CUBIC},
      // {"Value (delaunay)", hmap::NoiseType::VALUE_DELAUNAY},
      {"Value (linear)", hmap::NoiseType::VALUE_LINEAR},
      {"Worley", hmap::NoiseType::WORLEY},
      // {"Worley (doube)", hmap::NoiseType::WORLEY_DOUBLE},
      // {"Worley (value)", hmap::NoiseType::WORLEY_VALUE}
  };

  const std::map<std::string, int> noise_type_map_fbm = {
      {"Perlin", hmap::NoiseType::PERLIN},
      {"Perlin (billow)", hmap::NoiseType::PERLIN_BILLOW},
      {"Perlin (half)", hmap::NoiseType::PERLIN_HALF},
      {"OpenSimplex2", hmap::NoiseType::SIMPLEX2},
      // {"OpenSimplex2S", hmap::NoiseType::SIMPLEX2S},
      {"Value", hmap::NoiseType::VALUE},
      {"Value (cubic)", hmap::NoiseType::VALUE_CUBIC},
      {"Value (linear)", hmap::NoiseType::VALUE_LINEAR},
      {"Worley", hmap::NoiseType::WORLEY},
      // {"Worley (doube)", hmap::NoiseType::WORLEY_DOUBLE},
      // {"Worley (value)", hmap::NoiseType::WORLEY_VALUE}
  };

  const std::map<std::string, int> periodicity_type_map = {
      {"X-only", hmap::PeriodicityType::PERIODICITY_X},
      {"Y-only", hmap::PeriodicityType::PERIODICITY_Y},
      {"X and Y", hmap::PeriodicityType::PERIODICITY_XY}};

  const std::map<std::string, int> phasor_profile_map = {
      {"Cosine Bulky", hmap::PhasorProfile::PP_COSINE_BULKY},
      {"Cosine Peaky", hmap::PhasorProfile::PP_COSINE_PEAKY},
      {"Cosine Square", hmap::PhasorProfile::PP_COSINE_SQUARE},
      {"Cosine", hmap::PhasorProfile::PP_COSINE_STD},
      {"Triangle", hmap::PhasorProfile::PP_TRIANGLE},
      {"Dune", hmap::PhasorProfile::PP_DUNE}};

  const std::map<std::string, int> primitive_type_map = {
      {"Biquad Pulse", hmap::PrimitiveType::PRIM_BIQUAD_PULSE},
      {"Cone Smooth", hmap::PrimitiveType::PRIM_CONE_SMOOTH},
      {"Cubic Pulse", hmap::PrimitiveType::PRIM_CUBIC_PULSE},
      {"Smooth Cosine", hmap::PrimitiveType::PRIM_SMOOTH_COSINE}};

  const std::map<std::string, int> radial_profile_map = {
      {"Gain", hmap::RadialProfile::RP_GAIN},
      {"Linear", hmap::RadialProfile::RP_LINEAR},
      {"Power Law", hmap::RadialProfile::RP_POW},
      {"Smoothstep", hmap::RadialProfile::RP_SMOOTHSTEP},
      {"Smoothstep Upper", hmap::RadialProfile::RP_SMOOTHSTEP_UPPER},
      {"Flat Bottom", hmap::RadialProfile::RP_FLAT_BOTTOM},
      {"Square Root", hmap::RadialProfile::RP_SQRT},
  };

  const std::map<std::string, int> stamping_blend_method_map = {
      {"add", hmap::StampingBlendMethod::ADD},
      {"maximum", hmap::StampingBlendMethod::MAXIMUM},
      // {"maximum_smooth", hmap::StampingBlendMethod::MAXIMUM_SMOOTH},
      {"minimum", hmap::StampingBlendMethod::MINIMUM},
      // {"minimum_smooth", hmap::StampingBlendMethod::MINIMUM_SMOOTH},
      {"multiply", hmap::StampingBlendMethod::MULTIPLY},
      {"substract", hmap::StampingBlendMethod::SUBSTRACT}};

  const std::map<std::string, int> voronoi_return_type_map = {
      {"F1: squared distance to the closest point", hmap::VoronoiReturnType::F1_SQUARED},
      {"F2: squared distance to the second closest point",
       hmap::VoronoiReturnType::F2_SQUARED},
      {"F1 * F2: squared", hmap::VoronoiReturnType::F1TF2_SQUARED},
      {"F1 / F2: squared", hmap::VoronoiReturnType::F1DF2_SQUARED},
      {"F2 - F1: squared", hmap::VoronoiReturnType::F2MF1_SQUARED},
      {"Edge distance exp. decay", hmap::VoronoiReturnType::EDGE_DISTANCE_EXP},
      {"Edge distance squared", hmap::VoronoiReturnType::EDGE_DISTANCE_SQUARED},
      {"Cell value", hmap::VoronoiReturnType::CONSTANT},
      {"Cell value * (F2 - F1)", hmap::VoronoiReturnType::CONSTANT_F2MF1_SQUARED}};
} enum_mappings;

} // namespace hesiod