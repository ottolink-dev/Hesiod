/* Copyright (c) 2024 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

// Compile-only exercise of the (otherwise dormant) compat layer. Instantiates
// every legacy_traits specialization's create() overloads plus a handle, so the
// header is type-checked by the build even though no node includes it yet.
#include "hesiod/model/nodes/compat_attributes.hpp"

namespace hesiod::compat_check
{
void compile_check(meta::AttributeContainer &c)
{
  using namespace hsd::compat;
  legacy_traits<FloatAttribute>::create(c, "f", "F", 0.5f);
  legacy_traits<FloatAttribute>::create(c, "f2", "F", 0.5f, 0.f, 1.f, "{:.2f}", true);
  legacy_traits<IntAttribute>::create(c, "i", "I", 3);
  legacy_traits<BoolAttribute>::create(c, "b", "B", true);
  legacy_traits<BoolAttribute>::create(c, "b2", "B", "on", "off", false);
  std::map<std::string, int> m = {{"a", 0}, {"z", 3}};
  legacy_traits<EnumAttribute>::create(c, "e", "E", m);
  legacy_traits<EnumAttribute>::create(c, "e2", "E", m, "z");
  legacy_traits<SeedAttribute>::create(c, "s");
  legacy_traits<SeedAttribute>::create(c, "s2", "Seed", 42u);
  legacy_traits<RangeAttribute>::create(c, "r", "R", false);
  legacy_traits<RangeAttribute>::create(c, "r2", "R", glm::vec2(0.f, 1.f), -1.f, 2.f);
  legacy_traits<WaveNbAttribute>::create(c, "w", "W", glm::vec2(2.f, 2.f), 0.f, 64.f);
  legacy_traits<Vec2FloatAttribute>::create(c, "v", "V");
  legacy_traits<Vec2FloatAttribute>::create(c,
                                            "v2",
                                            "V",
                                            glm::vec2(0.5f, 0.5f),
                                            0.f,
                                            1.f,
                                            0.f,
                                            1.f);
  legacy_traits<CloudAttribute>::create(c, "cl", "C");
  legacy_traits<CloudAttribute>::create(c, "cl2", "C", true);
  legacy_traits<CloudAttribute>::create(c, "cl3", "C", std::vector<glm::vec3>{});
  legacy_traits<ColorAttribute>::create(c, "co", "C", 1.f, 0.f, 0.f, 1.f);
  legacy_traits<ColorAttribute>::create(c, "co2", "C", std::array<float, 4>{1, 1, 1, 1});
  legacy_traits<ColorGradientAttribute>::create(c, "cg", "G");
  legacy_traits<ColorGradientAttribute>::create(c, "cg2", "G", std::vector<Stop>{});
  legacy_traits<FilenameAttribute>::create(c, "fn", "F", "out.png", "PNG (*.png)", true);
  legacy_traits<StringAttribute>::create(c, "st", "S", "hello");
  legacy_traits<StringAttribute>::create(c, "st2", "S", "hello", true);
  legacy_traits<ChoiceAttribute>::create(c,
                                         "ch",
                                         "C",
                                         std::vector<std::string>{"x", "y"});
  legacy_traits<ChoiceAttribute>::create(c,
                                         "ch2",
                                         "C",
                                         std::vector<std::string>{"x", "y"},
                                         "y");
  legacy_traits<ChoiceAttribute>::create(c,
                                         "ch3",
                                         std::vector<std::string>{"x", "y"},
                                         "y");
  legacy_traits<VecFloatAttribute>::create(c,
                                           "vf",
                                           "V",
                                           std::vector<float>(8, 0.5f),
                                           0.f,
                                           1.f);

  // handles
  RangeHandle h(nullptr);
  (void)h;
  ChoiceHandle ch(nullptr);
  (void)ch;
  StringHandle sh(nullptr);
  (void)sh;
  FilenameHandle fh(nullptr);
  (void)fh;
  BoolHandle bh(nullptr);
  (void)bh;

  // concept + handle_of smoke check
  static_assert(CompatTag<FloatAttribute>);
  static_assert(CompatTag<ColorGradientAttribute>);
}
} // namespace hesiod::compat_check
