/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QVBoxLayout>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/heightmapper_widget.hpp"
#include "hesiod/logger.hpp"

static constexpr const char
    *heightmapper_url = "https://tangrams.github.io/heightmapper/"; // TODO move to config

namespace hesiod
{

HeightmapperWidget::HeightmapperWidget(QWidget *parent) : QWidget(parent)
{
  Logger::log()->trace("HeightmapperWidget::HeightmapperWidget");

  this->setWindowTitle(
      "Hesiod - Tangram Heightmapper (https://tangrams.github.io/heightmapper)");

  // use an off-the-record profile so the page can still make
  // network requests, but nothing is written to disk by Qt itself.
  this->profile = new QWebEngineProfile(this);
  this->profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);

  auto *page = new QWebEnginePage(this->profile, this->profile);
  this->view = new QWebEngineView(this);
  this->view->setPage(page);

  this->setMinimumSize(QSize(768, 768));

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(this->view);

  this->setup_download_interception();

  this->view->load(QUrl(heightmapper_url));

  // restore window geometry
  this->restore_window_state();
}

void HeightmapperWidget::closeEvent(QCloseEvent *event)
{
  this->save_window_state();
  QWidget::closeEvent(event);
}

void HeightmapperWidget::setup_download_interception()
{
  Logger::log()->trace("HeightmapperWidget::setup_download_interception");

  // QWebEngineProfile::downloadRequested fires whenever the page
  // triggers a file download (e.g. the user clicks "Export" in
  // heightmapper).
  connect(this->profile,
          &QWebEngineProfile::downloadRequested,
          this,
          &HeightmapperWidget::on_download_requested);
}

void HeightmapperWidget::on_download_requested(QWebEngineDownloadRequest *download)
{
  Logger::log()->trace("HeightmapperWidget::on_download_requested");

  // capture to memory instead of writing to disk.
  download->setSavePageFormat(QWebEngineDownloadRequest::UnknownSaveFormat);

  const QString suggested_name = download->suggestedFileName();
  const QString mime_type = download->mimeType();

  Q_EMIT heightmap_download_started(suggested_name);

  // buffer the incoming bytes in a QByteArray via QBuffer.
  auto *buffer = new QBuffer(this);
  buffer->open(QIODevice::WriteOnly);
  download->setDownloadDirectory(QString()); // keep empty → memory path

  // Qt6 delivers data through QWebEngineDownloadRequest::isFinished /
  // stateChanged. We accumulate via the receivedBytes change and read
  // from the temp file once the state reaches Completed.

  // simpler pattern: let Qt write to a temp path we own, then read it
  // back.
  const QString tmp_path = QDir::tempPath() + QDir::separator() + suggested_name;
  download->setDownloadDirectory(QDir::tempPath());
  download->setDownloadFileName(suggested_name);
  download->accept();

  connect(download,
          &QWebEngineDownloadRequest::stateChanged,
          this,
          [this, download, tmp_path, suggested_name, mime_type, buffer](
              QWebEngineDownloadRequest::DownloadState state)
          {
            switch (state)
            {
            case QWebEngineDownloadRequest::DownloadCompleted:
            {
              QFile file(tmp_path);
              if (file.open(QIODevice::ReadOnly))
              {
                DownloadInfo info;
                info.suggested_filename = suggested_name;
                info.mime_type = mime_type;
                info.data = file.readAll();
                file.close();
                file.remove(); // clean up the temp file

                Q_EMIT this->heightmap_download_ready(info);
              }
              else
              {
                Q_EMIT this->heightmap_download_failed(suggested_name);
              }
              buffer->deleteLater();
              break;
            }

            case QWebEngineDownloadRequest::DownloadCancelled:
            case QWebEngineDownloadRequest::DownloadInterrupted:
              Q_EMIT this->heightmap_download_failed(suggested_name);
              buffer->deleteLater();
              break;

            default:
              break;
            }
          });
}

void HeightmapperWidget::reload()
{
  this->view->triggerPageAction(QWebEnginePage::ReloadAndBypassCache);
}

void HeightmapperWidget::restore_window_state()
{
  AppContext &ctx = HSD_CTX;

  this->setGeometry(ctx.app_settings.window.geom_heightmapper.x,
                    ctx.app_settings.window.geom_heightmapper.y,
                    ctx.app_settings.window.geom_heightmapper.w,
                    ctx.app_settings.window.geom_heightmapper.h);
}

void HeightmapperWidget::save_window_state() const
{
  AppContext &ctx = HSD_CTX;

  QRect geom = this->geometry();
  ctx.app_settings.window.geom_heightmapper.x = geom.x();
  ctx.app_settings.window.geom_heightmapper.y = geom.y();
  ctx.app_settings.window.geom_heightmapper.w = geom.width();
  ctx.app_settings.window.geom_heightmapper.h = geom.height();
}

} // namespace hesiod
