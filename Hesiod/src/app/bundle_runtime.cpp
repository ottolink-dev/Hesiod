#include <stdexcept>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QStandardPaths>

#include "hesiod/app/bundle_runtime.hpp"

namespace hesiod
{
namespace
{
QString launch_directory;
}

void initialize_bundle_runtime()
{
#ifdef Q_OS_MACOS
  const QDir executable(QCoreApplication::applicationDirPath());
  const QString resources = QFileInfo(executable.filePath("../Resources/data"))
                                .canonicalFilePath();
  if (resources.isEmpty() || executable.dirName() != "MacOS")
    return;

  // A workspace per installed location avoids mixing assets between concurrent
  // copies of the application. Nothing in the signed bundle is ever modified.
  const QString key = QString::fromLatin1(QCryptographicHash::hash(
      resources.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
  const QString storage = QStandardPaths::writableLocation(
      QStandardPaths::AppLocalDataLocation);
  if (storage.isEmpty())
    throw std::runtime_error("Cannot locate Hesiod's application support directory");
  const QDir workspace(storage + "/runtime/" + key);
  if (!QDir().mkpath(workspace.path()))
    throw std::runtime_error("Cannot create Hesiod's runtime workspace");

  QLockFile lock(workspace.filePath(".assets.lock"));
  if (!lock.tryLock(10000))
    throw std::runtime_error("Cannot lock Hesiod's runtime workspace");
  const QString link = workspace.filePath("data");
  const QFileInfo existing(link);
  if (existing.isSymLink())
  {
    if (existing.symLinkTarget() != resources && !QFile::remove(link))
      throw std::runtime_error("Cannot update Hesiod's runtime assets link");
  }
  else if (existing.exists())
    throw std::runtime_error("Hesiod's runtime assets path is not a symbolic link");

  if (!QFileInfo(link).isSymLink() && !QFile::link(resources, link))
    throw std::runtime_error("Cannot link Hesiod's bundled assets");

  launch_directory = QDir::currentPath();
  if (!QDir::setCurrent(workspace.path()))
    throw std::runtime_error("Cannot enter Hesiod's runtime workspace");
#endif
}

std::string resolve_launch_path(const std::string &path)
{
  if (path.empty() || launch_directory.isEmpty())
    return path;
  return QDir(launch_directory).absoluteFilePath(QString::fromStdString(path))
      .toStdString();
}
} // namespace hesiod
