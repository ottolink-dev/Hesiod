/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QSplashScreen>

namespace hesiod
{

class SplashScreen : public QSplashScreen
{
public:
  SplashScreen();

  void close();
  void show_message(const QString &message);
};

} // namespace hesiod