/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QVBoxLayout>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/heightmapper_widget.hpp"
#include "hesiod/logger.hpp"

static const QList<QPair<QString, QString>> source_urls = {
    {"Manticorp",
     "https://manticorp.github.io/unrealheightmap/#width/1024/height/1024/outputformat/"
     "png16"},
    {"Tangram Heightmapper", "https://tangrams.github.io/heightmapper"},
};

namespace hesiod
{

HeightmapperWidget::HeightmapperWidget(QWidget *parent) : QWidget(parent)
{
  Logger::log()->trace("HeightmapperWidget::HeightmapperWidget");

  this->setWindowTitle(
      "Hesiod - Tangram Heightmapper (https://tangrams.github.io/heightmapper)");

  // The view is created before the profile on purpose: QObject destroys its
  // children in creation order, and QtWebEngine requires the profile to
  // outlive every page and view using it. The page is parented to the view for
  // the same reason, so that view and page go together, ahead of the profile.
  // See ~HeightmapperWidget.
  this->view = new QWebEngineView(this);

  // use an off-the-record profile so the page can still make
  // network requests, but nothing is written to disk by Qt itself.
  this->profile = new QWebEngineProfile(this);
  this->profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);

  this->view->setPage(new QWebEnginePage(this->profile, this->view));

  this->setMinimumSize(QSize(768, 768));

  // --- combo box ---
  this->url_combo = new QComboBox(this);
  for (const auto &[label, url] : source_urls)
    this->url_combo->addItem(label, url);

  connect(this->url_combo,
          &QComboBox::currentIndexChanged,
          this,
          &HeightmapperWidget::on_source_changed);

  // --- layout ---
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);
  layout->addWidget(this->url_combo);
  layout->addWidget(this->view);

  this->setup_download_interception();

  // load first entry
  this->view->load(QUrl(source_urls.first().second));

  this->restore_window_state();
}

HeightmapperWidget::~HeightmapperWidget()
{
  Logger::log()->trace("HeightmapperWidget::~HeightmapperWidget");

  // Delete the view (and with it its page) before the profile, which is what
  // QtWebEngine requires and what construction order already arranges. Doing it
  // explicitly keeps the guarantee if those lines are ever reordered.
  //
  // Get it wrong and QWebEngineProfile's destructor releases the profile with a
  // live page attached: it logs "Release of profile requested but WebEnginePage
  // still not deleted. Expect troubles !", and when the page owns a live render
  // process the profile's cleanup - run from qt_call_post_routines inside
  // ~QApplication - blocks in WaitForSingleObject and the process never exits.
  delete this->view;
  this->view = nullptr;
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

void HeightmapperWidget::on_source_changed(int index)
{
  const QString url = this->url_combo->itemData(index).toString();
  if (!url.isEmpty())
    this->view->load(QUrl(url));
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
