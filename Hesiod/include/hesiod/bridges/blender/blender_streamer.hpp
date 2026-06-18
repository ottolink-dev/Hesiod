/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <cstdint>

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

namespace hesiod
{

class BlenderStreamer : public QObject
{
  Q_OBJECT

public:
  void start(std::uint16_t port = 9001);

  std::uint16_t get_port() const;

  void send_heightmap(const float *data, int width, int height);

private:
  bool          started = false;
  std::uint16_t current_port = 9001;
  QTcpServer    server;
  QTcpSocket   *client_socket = nullptr;
};

} // namespace hesiod