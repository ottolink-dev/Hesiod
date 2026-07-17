/* Copyright (c) 2024 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

/* Node-facing compatibility header: legacy attr:: names backed by Meta storage.
   Node files include this instead of the legacy "attributes.hpp".
   NEVER include this in a TU that also includes "attributes.hpp" (brush.cpp,
   base_node.*, node_attributes_widget.*) — the attr:: names would collide. */
#pragma once
#include "hesiod/model/nodes/compat/legacy_compat.hpp"

namespace attr
{
using hsd::compat::FloatAttribute;
using hsd::compat::IntAttribute;
using hsd::compat::BoolAttribute;
using hsd::compat::EnumAttribute;
using hsd::compat::SeedAttribute;
using hsd::compat::RangeAttribute;
using hsd::compat::WaveNbAttribute;
using hsd::compat::Vec2FloatAttribute;
using hsd::compat::CloudAttribute;
using hsd::compat::ColorAttribute;
using hsd::compat::ColorGradientAttribute;
using hsd::compat::FilenameAttribute;
using hsd::compat::StringAttribute;
using hsd::compat::ChoiceAttribute;
using hsd::compat::VecFloatAttribute;
using hsd::compat::Stop;
} // namespace attr
