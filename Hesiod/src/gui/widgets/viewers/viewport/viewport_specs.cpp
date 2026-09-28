/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cctype>

#include "qtr/keys.hpp"
#include "qtr/water_colors.hpp"

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

Spec aspec(std::string key,
           std::string label,
           std::string category,
           float       vmin,
           float       vmax)
{
  Spec s = fspec(std::move(key),
                 std::move(label),
                 std::move(category),
                 vmin,
                 vmax,
                 "{:.1f}");
  s.angle = true;
  return s;
}

Spec bspec(std::string key, std::string label, std::string category, bool invert = false)
{
  Spec s;
  s.type = Spec::Type::Bool;
  s.key = std::move(key);
  s.label = std::move(label);
  s.category = std::move(category);
  s.invert = invert;
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

Spec chspec(std::string                              key,
            std::string                              label,
            std::string                              category,
            std::vector<std::pair<int, std::string>> items)
{
  Spec s;
  s.type = Spec::Type::Choice;
  s.key = std::move(key);
  s.label = std::move(label);
  s.category = std::move(category);
  s.items = std::move(items);
  return s;
}

} // namespace

std::string visible_key(const std::string &mesh) { return "visible." + mesh; }

std::vector<Spec> specs_for(int tool)
{
  switch (tool)
  {
  case ToolLighting:
    return {aspec("light_phi", "Sun Azimuth", "Sun", -180.f, 180.f),
            aspec("light_theta", "Sun Elevation", "Sun", 0.f, 90.f),
            bspec("auto_rotate_light", "Auto Rotate", "Sun"),
            bspec("bypass_shadow_map", "Shadows", "Light", true),
            fspec("shadow_strength", "Shadow Strength", "Light", 0.f, 1.f),
            chspec("shadow_map_resolution",
                   "Shadow Resolution",
                   "Light",
                   {{512, "512"},
                    {1024, "1024"},
                    {2048, "2048"},
                    {4096, "4096"},
                    {8192, "8192"}}),
            bspec("add_ambiant_occlusion", "Ambient Occlusion", "Light"),
            fspec("ambiant_occlusion_strength", "AO Strength", "Light", 0.f, 1.f),
            fspec("ambiant_occlusion_radius", "AO Radius", "Light", 0.f, 0.5f, "{:.3f}")};

  case ToolCamera:
  {
    Spec fov = aspec("fov", "Field of View", "Camera", 10.f, 180.f);
    fov.format = "{:.0f}";
    return {fov,
            fspec("scale_h", "Height Scale", "Camera", 0.f, 2.f),
            bspec("show_orientation_gizmo", "Orientation Gizmo", "Camera"),
            bspec("keyboard_navigation_enabled", "Keyboard Controls", "Navigation"),
            chspec("keyboard_layout",
                   "Layout",
                   "Navigation",
                   {{0, "WASD (QWERTY)"}, {1, "ZQSD (AZERTY)"}}),
            fspec("camera_move_speed", "Move Speed", "Navigation", 0.1f, 10.f)};
  }

  case ToolDisplay:
    // terrain, water, points and path visibility are the Preview panel's eyes
    return {bspec(visible_key(qtr::keys::mesh::plane), "Ground Plane", "Layers"),
            bspec("wireframe_mode", "Wireframe", "Debug"),
            bspec("normal_visualization", "Normals", "Debug")};

  case ToolMaterial:
    // the albedo texture on/off is the Preview panel's texture eye
    return {fspec("gamma_correction", "Gamma", "Albedo", 0.01f, 4.f),
            bspec("apply_tonemap", "Tonemap", "Albedo"),
            fspec("normal_map_scaling", "Normal Strength", "Normal Map", 0.f, 2.f)};

  case ToolWater:
  {
    Spec preset;
    preset.type = Spec::Type::WaterPreset;
    preset.key = "water_preset";
    preset.label = "Preset";
    preset.category = "Water";
    int i = 0;
    for (const auto &[name, _] : qtr::water_colors)
    {
      std::string pretty = name;
      std::replace(pretty.begin(), pretty.end(), '_', ' ');
      if (!pretty.empty())
        pretty[0] = char(std::toupper(pretty[0]));
      preset.items.push_back({i++, pretty});
    }

    Spec angle = aspec("waves_alpha", "Wave Angle", "Waves", -180.f, 180.f);
    return {
        preset,
        cspec("color_shallow_water", "Shallow Color", "Water"),
        cspec("color_deep_water", "Deep Color", "Water"),
        fspec("water_color_depth", "Color Depth", "Water", 0.f, 0.2f, "{:.3f}"),
        fspec("water_spec_strength", "Specularity", "Water", 0.f, 1.f),
        bspec("add_water_foam", "Foam", "Foam"),
        fspec("foam_depth", "Foam Depth", "Foam", 0.f, 0.1f, "{:.3f}"),
        bspec("add_water_waves", "Waves", "Waves"),
        fspec("waves_kw", "Wavenumber", "Waves", 0.f, 2048.f, "{:.0f}"),
        fspec("waves_amplitude", "Amplitude", "Waves", 0.f, 0.1f, "{:.3f}"),
        fspec("waves_normal_amplitude", "Normal Amplitude", "Waves", 0.f, 0.1f, "{:.3f}"),
        angle,
        fspec("angle_spread_ratio", "Angle Spread", "Waves", 0.f, 0.1f, "{:.3f}"),
        bspec("animate_waves", "Animate", "Waves"),
        fspec("waves_speed", "Speed", "Waves", 0.f, 1.f)};
  }

  case ToolSky:
    return {chspec("background_mode",
                   "Background",
                   "Background",
                   {{0, "Sky"}, {1, "Void: black, with a grid"}}),
            bspec("show_skybox", "Skybox", "Sky"),
            chspec("skybox_mode", "Mode", "Sky", {{0, "Uniform color"}, {1, "Image"}}),
            cspec("skybox_color", "Sky Color", "Sky"),
            aspec("skybox_rotation", "Rotation", "Sky", -180.f, 180.f),
            bspec("add_fog", "Fog", "Fog"),
            fspec("fog_density", "Density", "Fog", 0.f, 100.f, "{:.1f}"),
            fspec("fog_height", "Height", "Fog", 0.f, 1.f),
            bspec("fog_match_skybox", "Match Skybox", "Fog"),
            cspec("fog_color", "Fog Color", "Fog"),
            bspec("add_atmospheric_scattering", "Scattering", "Scattering"),
            fspec("scattering_density", "Density", "Scattering", 0.f, 1.f),
            fspec("fog_strength", "Fog Strength", "Scattering", 0.f, 1.f),
            fspec("fog_scattering_ratio", "Scattering Ratio", "Scattering", 0.f, 1.f),
            cspec("rayleigh_color", "Rayleigh Color", "Scattering"),
            cspec("mie_color", "Mie Color", "Scattering")};

  case ToolLighting2D:
  {
    Spec elevation = aspec("2d.sun_zenith", "Sun Elevation", "Sun", 0.f, 90.f);
    elevation.elevation_zenith = true;
    return {aspec("2d.sun_azimuth", "Sun Azimuth", "Sun", -180.f, 180.f),
            elevation,
            bspec("2d.hillshading", "Hillshading", "Sun")};
  }

  case ToolColormap2D:
    return {chspec("2d.colormap",
                   "Colormap",
                   "Display",
                   {{0, "Gray"}, {1, "Viridis"}, {2, "Turbo"}, {3, "Magma"}})};

  default:
    return {};
  }
}

} // namespace hesiod::viewport
