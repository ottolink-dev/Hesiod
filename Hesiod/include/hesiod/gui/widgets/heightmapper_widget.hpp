/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once

#include <QByteArray>
#include <QComboBox>
#include <QString>
#include <QWebEngineDownloadRequest>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWidget>

namespace hesiod
{

// =====================================
// HeightmapperWidget
// =====================================

class HeightmapperWidget : public QWidget
{
  Q_OBJECT

public:
  /// --- Metadata attached to each download
  struct DownloadInfo
  {
    QString    suggested_filename; ///< Filename proposed by the browser
    QString    mime_type;          ///< MIME type reported by the page
    QByteArray data;               ///< Raw file bytes (PNG for heightmaps)
  };

  // --- Ctor
  explicit HeightmapperWidget(QWidget *parent = nullptr);
  ~HeightmapperWidget() override = default;

signals:
  // --- Downloads management
  void heightmap_download_ready(const HeightmapperWidget::DownloadInfo &info);
  void heightmap_download_started(const QString &suggested_filename);
  void heightmap_download_failed(const QString &suggested_filename);

public slots:
  void reload();

private slots:
  void on_download_requested(QWebEngineDownloadRequest *download);
  void on_source_changed(int index);

  // --- Qt Events
  void closeEvent(QCloseEvent *event) override;

private:
  void setup_download_interception();

  // --- State Management
  void restore_window_state();
  void save_window_state() const;

  // --- Members
  QWebEngineView    *view = nullptr;
  QWebEngineProfile *profile = nullptr;
  QComboBox         *url_combo = nullptr;
};

} // namespace hesiod