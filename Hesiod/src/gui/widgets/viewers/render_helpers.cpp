/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <format>

#include "highmap/array.hpp"
#include "highmap/operator.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/profiling.hpp"

namespace hesiod
{

std::vector<uint8_t> generate_selector_image(hmap::Array &array)
{
  HSD_PROFILE_ZONE("Viewer::selector_image");
  HSD_PROFILE_TEXT(std::format("shape={}x{}", array.shape.x, array.shape.y));
  std::vector<uint8_t> img(4 * array.shape.x * array.shape.y);

  int k = 0;
  for (int j = 0; j < array.shape.y; ++j)
    for (int i = 0; i < array.shape.x; i++)
    {
      img[k++] = (uint8_t)(255.f);
      img[k++] = (uint8_t)(255.f * (1.f - array(i, j)));
      img[k++] = (uint8_t)(255.f);
      img[k++] = (uint8_t)(255.f);
    }

  return img;
}

} // namespace hesiod
