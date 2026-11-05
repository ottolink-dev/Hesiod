/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include <QApplication>
#include <QPainter>
#include <QPropertyAnimation>

#include "hesiod/gui/widgets/dialogs/splash_screen.hpp"

namespace hesiod
{

SplashScreen::SplashScreen()
{
  // retrieve version - hardcoded since settings not set at this
  // point...
  std::ifstream f("data/git_version.txt");
  std::string   version = "???";
  if (f.is_open())
    version = std::string((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());

  // load and show splash
  QPixmap pixmap("data/splash.png");

  QPainter p(&pixmap);
  p.setPen(Qt::lightGray);
  p.drawText(20, 24, version.c_str());
  p.end();

  this->setPixmap(pixmap);
  this->show();
  QApplication::processEvents();
}

void SplashScreen::close()
{
  // Fade out animation
  QPropertyAnimation *anim = new QPropertyAnimation(this, "windowOpacity");

  anim->setDuration(1000);
  anim->setStartValue(1.f);
  anim->setEndValue(0.f);
  anim->setEasingCurve(QEasingCurve::Linear);

  QObject::connect(anim,
                   &QPropertyAnimation::finished,
                   this,
                   &QSplashScreen::deleteLater);

  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void SplashScreen::show_message(const QString &message)
{
  this->showMessage(message, Qt::AlignBottom | Qt::AlignCenter, Qt::white);
  QApplication::processEvents();
}
} // namespace hesiod
