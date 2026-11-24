/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/widgets/viewers/viewport/viewport_specs.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_style.hpp"

namespace hesiod::viewport
{

namespace
{

Spec fspec(std::string key,
           std::string label,
           std::string category,
           float       vmin,
           float       vmax,
           std::string format = "{:.2f}")
{
  Spec s;
  s.type = Spec::Type::Float;
  s.key = std::move(key);
  s.label = std::move(label);
  s.category = std::move(category);
  s.vmin = vmin;
  s.vmax = vmax;
  s.format = std::move(format);
  return s;
}

Spec bspec(std::string key, std::string label, std::string category)
{
  Spec s;
  s.type = Spec::Type::Bool;
  s.key = std::move(key);
  s.label = std::move(label);
  s.category = std::move(category);
  return s;
}

Spec cspec(std::string key, std::string label, std::string category)
{
  Spec s;
  s.type = Spec::Type::Color;
  s.key = std::move(key);
  s.label = std::move(label);
  s.category = std::move(category);
  return s;
}

} // namespace

std::vector<Spec> specs_for(int tool)
{
  switch (tool)
  {
  case ToolLighting:
    return {fspec("sun.azimuth_deg", "Sun Azimuth", "Sun", -180.f, 180.f, "{:.1f}"),
            fspec("sun.elevation_deg", "Sun Elevation", "Sun", -5.f, 90.f, "{:.1f}"),
            fspec("sun.intensity", "Sun Intensity", "Sun", 10000.f, 200000.f, "{:.0f}"),
            bspec("sun.auto_color_temperature", "Auto Color Temp", "Sun"),
            bspec("env.soft_shadows_enabled", "Soft Shadows", "Light"),
            bspec("env.ambient_occlusion_enabled", "Ambient Occlusion", "Light")};

  case ToolCamera:
    return {
        fspec("cam.fov_deg", "Field of View", "Camera", 10.f, 120.f, "{:.0f}"),
        fspec("terrain.height_scale_multiplier", "Height Scale", "Camera", 0.1f, 3.f)};

  case ToolDisplay:
    return {bspec("env.ground_plane_enabled", "Ground Plane", "Layers"),
            fspec("env.ground_plane_elevation",
                  "Ground Elevation",
                  "Layers",
                  -100.f,
                  200.f,
                  "{:.1f}"),
            cspec("env.ground_plane_color", "Ground Color", "Layers"),
            bspec("cdlod.wireframe_enabled", "Wireframe", "Debug"),
            bspec("cdlod.morphing_enabled", "CDLOD Morphing", "Debug"),
            bspec("cdlod.lod_color_debug_enabled", "LOD Level Colors", "Debug"),
            fspec("cdlod.lod_distance_scale", "LOD Distance", "Debug", 0.2f, 4.f)};

  case ToolMaterial:
    return {bspec("env.pbr_enabled", "Full PBR", "Shading"),
            bspec("cdlod.shading_enabled", "Terrain Shading", "Shading")};

  case ToolWater:
    return {cspec("water.shallow_color", "Shallow Color", "Water"),
            cspec("water.deep_color", "Deep Color", "Water"),
            fspec("water.depth_falloff", "Depth Falloff", "Water", 0.5f, 30.f, "{:.1f}"),
            fspec("water.opacity", "Opacity", "Water", 0.1f, 1.f),
            fspec("water.roughness", "Roughness", "Water", 0.04f, 1.f)};

  case ToolSky:
    return {
        bspec("env.atmospheric_scattering_enabled", "Atmospheric Sky", "Sky"),
        bspec("sun.render_sun_disk", "Sun Disk", "Sky"),
        fspec("env.atmosphere_turbidity", "Turbidity", "Sky", 1.f, 10.f),
        fspec("env.rayleigh_strength", "Rayleigh", "Sky", 0.f, 3.f),
        fspec("env.mie_strength", "Mie", "Sky", 0.f, 3.f),
        bspec("env.fog_enabled", "Height Fog", "Fog"),
        fspec("env.fog_density", "Density", "Fog", 0.f, 0.005f, "{:.4f}"),
        fspec("env.fog_start", "Start Distance", "Fog", 0.f, 500.f, "{:.0f}"),
        fspec("env.fog_height", "Height", "Fog", 0.f, 500.f, "{:.0f}"),
        fspec("env.fog_height_falloff", "Height Falloff", "Fog", 0.001f, 0.1f, "{:.3f}"),
        cspec("view.background_color", "Background Color", "Background")};

  default:
    return {};
  }
}

} // namespace hesiod::viewport
