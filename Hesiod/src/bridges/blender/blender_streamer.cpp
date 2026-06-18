/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QByteArray>
#include <QDataStream>

#include <zlib.h>

#include "hesiod/bridges/blender/blender_streamer.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

std::uint16_t BlenderStreamer::get_port() const { return this->current_port; }

void BlenderStreamer::start(std::uint16_t port)
{
  Logger::log()->trace("BlenderStreamer::start: port {} (current_port: {})",
                       port,
                       this->current_port);

  if (this->started && port == this->current_port)
  {
    Logger::log()->trace("BlenderStreamer::start: already started");
    return;
  }

  this->connect(&this->server,
                &QTcpServer::newConnection,
                this,
                [this]()
                {
                  if (this->client_socket)
                    this->client_socket->deleteLater();

                  this->client_socket = this->server.nextPendingConnection();

                  qDebug() << "Blender connected";

                  this->connect(this->client_socket,
                                &QTcpSocket::disconnected,
                                this,
                                [this]()
                                {
                                  qDebug() << "Blender disconnected";
                                  this->client_socket->deleteLater();
                                  this->client_socket = nullptr;
                                });
                });

  this->server.listen(QHostAddress::Any, port);
  this->started = true;
  this->current_port = port;
}

void BlenderStreamer::send_heightmap(const float *data, int width, int height)
{
  if (!this->started)
    this->start();

  if (!this->client_socket)
    return;

  const size_t raw_size = width * height * sizeof(float);
  uLongf       compressed_size = compressBound(raw_size);

  QByteArray compressed;
  compressed.resize(compressed_size);

  int status = compress(reinterpret_cast<Bytef *>(compressed.data()),
                        &compressed_size,
                        reinterpret_cast<const Bytef *>(data),
                        raw_size);

  if (status != Z_OK)
    return;

  compressed.resize(compressed_size);

  QByteArray  packet;
  QDataStream stream(&packet, QIODevice::WriteOnly);

  stream.setByteOrder(QDataStream::LittleEndian);

  stream << width;
  stream << height;
  stream << static_cast<qint32>(compressed_size);

  packet.append(compressed);

  this->client_socket->write(packet);
}

} // namespace hesiod
