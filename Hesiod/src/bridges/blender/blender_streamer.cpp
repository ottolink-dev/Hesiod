#include <QByteArray>
#include <QTcpServer>
#include <QTcpSocket>
#include <cstdint>
#include <zlib.h>

#include "hesiod/bridges/blender/blender_streamer.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

std::uint16_t BlenderStreamer::get_port() const { return this->current_port; }

void BlenderStreamer::start(std::uint16_t port)
{
  Logger::log()->trace("BlenderStreamer::start: port {}", port);

  if (this->started && port == this->current_port)
    return;

  connect(&this->server,
          &QTcpServer::newConnection,
          this,
          [this]()
          {
            if (this->client_socket)
              this->client_socket->deleteLater();

            this->client_socket = this->server.nextPendingConnection();

            qDebug() << "Blender connected";

            connect(this->client_socket,
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

void BlenderStreamer::send_heightmap(const float *data, int width, int height, int id)
{
  if (!this->started)
    this->start();

  if (!this->client_socket)
    return;

  // --- Compress

  const size_t raw_size = width * height * sizeof(float);
  uLongf       compressed_max = compressBound(raw_size);

  QByteArray compressed;
  compressed.resize(compressed_max);

  int status = compress(reinterpret_cast<Bytef *>(compressed.data()),
                        &compressed_max,
                        reinterpret_cast<const Bytef *>(data),
                        raw_size);

  if (status != Z_OK)
    return;

  compressed.resize(compressed_max);

  // --- Header

  struct Header
  {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    uint32_t hm_size;
    uint32_t has_texture;
  } header;

  header.id = static_cast<uint32_t>(id);
  header.width = static_cast<uint32_t>(width);
  header.height = static_cast<uint32_t>(height);
  header.hm_size = static_cast<uint32_t>(compressed.size());
  header.has_texture = 0;

  // --- Send

  QByteArray packet;
  packet.append(reinterpret_cast<const char *>(&header), sizeof(Header));
  packet.append(compressed);

  this->client_socket->write(packet);
}

void BlenderStreamer::send_heightmap_and_texture(const float *h_data,
                                                 const float *rgba_data,
                                                 int          width,
                                                 int          height,
                                                 int          id)
{
  if (!this->started)
    this->start();

  if (!this->client_socket)
    return;

  // --- Compress heightmap

  const size_t hm_raw_size = width * height * sizeof(float);
  uLongf       hm_max = compressBound(hm_raw_size);

  QByteArray hm_compressed;
  hm_compressed.resize(hm_max);

  if (compress(reinterpret_cast<Bytef *>(hm_compressed.data()),
               &hm_max,
               reinterpret_cast<const Bytef *>(h_data),
               hm_raw_size) != Z_OK)
    return;

  hm_compressed.resize(hm_max);

  // --- Compress texture

  QByteArray rgba_compressed;
  uint32_t   rgba_size = 0;

  bool has_texture = (rgba_data != nullptr);

  if (has_texture)
  {
    const size_t rgba_raw_size = width * height * 4 * sizeof(float);
    uLongf       rgba_max = compressBound(rgba_raw_size);

    rgba_compressed.resize(rgba_max);

    if (compress(reinterpret_cast<Bytef *>(rgba_compressed.data()),
                 &rgba_max,
                 reinterpret_cast<const Bytef *>(rgba_data),
                 rgba_raw_size) != Z_OK)
      return;

    rgba_compressed.resize(rgba_max);
    rgba_size = static_cast<uint32_t>(rgba_compressed.size());
  }

  // --- Header

  struct Header
  {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    uint32_t hm_size;
    uint32_t has_texture;
  } header;

  header.id = static_cast<uint32_t>(id);
  header.width = static_cast<uint32_t>(width);
  header.height = static_cast<uint32_t>(height);
  header.hm_size = static_cast<uint32_t>(hm_compressed.size());
  header.has_texture = has_texture ? 1u : 0u;

  // --- Packet

  QByteArray packet;

  packet.append(reinterpret_cast<const char *>(&header), sizeof(Header));
  packet.append(hm_compressed);

  if (has_texture)
  {
    packet.append(reinterpret_cast<const char *>(&rgba_size), sizeof(uint32_t));
    packet.append(rgba_compressed);
  }

  // --- Packet

  this->client_socket->write(packet);
}

} // namespace hesiod
