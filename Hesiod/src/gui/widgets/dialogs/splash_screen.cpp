/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include <QApplication>
#include <QPainter>
#include <QPropertyAnimation>

#include "hesiod/gui/motion/motion.hpp"
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
  // with interface animations off the splash is simply gone
  if (!motion::enabled())
  {
    this->hide();
    this->deleteLater();
    return;
  }

  // Fade out animation: gone in a third of a second, gathering speed. A full
  // second of linear fade kept a ghost of the splash over a window that was
  // ready to use.
  QPropertyAnimation *anim = new QPropertyAnimation(this, "windowOpacity");

  anim->setDuration(motion::splash_out);
  anim->setStartValue(1.f);
  anim->setEndValue(0.f);
  anim->setEasingCurve(QEasingCurve::InCubic);

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
