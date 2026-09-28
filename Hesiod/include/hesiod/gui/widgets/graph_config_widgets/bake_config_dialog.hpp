/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
 * License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>
#include <vector>

#include <QPointer>

#include "hesiod/gui/widgets/message_dialog.hpp"
#include "hesiod/model/graph/bake_config.hpp"

class QLineEdit;

namespace meta
{
class AttributeContainer;
}

namespace hesiod
{

// =====================================
// BakeConfigDialog
// =====================================

// Bake and export settings, in the application's own dialog chrome. The
// settings are Meta attributes rendered with the properties panel's design, so
// they are the same animated section cards, sliders and switches as the node
// settings and the viewport panels.
class BakeConfigDialog : public MessageDialog
{
public:
  explicit BakeConfigDialog(int               max_size,
                            const BakeConfig &initial_value,
                            const QString    &default_dir, // shown when no folder is set
                            QWidget          *parent = nullptr);
  ~BakeConfigDialog() override;

  BakeConfig get_bake_settings() const;

private:
  // one container per section, so each section's note can sit under it
  std::unique_ptr<meta::AttributeContainer> output;
  std::unique_ptr<meta::AttributeContainer> options;
  std::unique_ptr<meta::AttributeContainer> memory;
  std::vector<QPointer<QWidget>>            rows; // subscribe to the containers
  QLineEdit                                *folder_edit = nullptr;
};

} // namespace hesiod
