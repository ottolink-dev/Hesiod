/* Copyright (c) 2024 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

/* Node-facing compatibility header: legacy attr:: names backed by Meta storage.
   Node files include this instead of the legacy "attributes.hpp".
   NEVER include this in a TU that also includes "attributes.hpp" (brush.cpp,
   base_node.*, node_attributes_widget.*) — the attr:: names would collide. */
#pragma once
#include "hesiod/model/nodes/legacy/legacy_compat.hpp"

namespace attr
{
using hsd::legacy::BoolAttribute;
using hsd::legacy::ChoiceAttribute;
using hsd::legacy::CloudAttribute;
using hsd::legacy::ColorAttribute;
using hsd::legacy::ColorGradientAttribute;
using hsd::legacy::EnumAttribute;
using hsd::legacy::FilenameAttribute;
using hsd::legacy::FloatAttribute;
using hsd::legacy::IntAttribute;
using hsd::legacy::RangeAttribute;
using hsd::legacy::SeedAttribute;
using hsd::legacy::Stop;
using hsd::legacy::StringAttribute;
using hsd::legacy::Vec2FloatAttribute;
using hsd::legacy::VecFloatAttribute;
using hsd::legacy::WaveNbAttribute;
} // namespace attr
