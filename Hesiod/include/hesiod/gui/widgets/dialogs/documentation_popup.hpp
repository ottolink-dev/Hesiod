/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>

#include <QWidget>

namespace hesiod
{

class DocumentationPopup : public QWidget
{
public:
  DocumentationPopup(const std::string &title,
                     const std::string &html_source,
                     QWidget           *parent = nullptr);
};

} // namespace hesiod