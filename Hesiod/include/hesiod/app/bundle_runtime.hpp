#pragma once

#include <string>

namespace hesiod
{
// Bundled macOS builds keep legacy relative asset paths in a writable workspace.
// Other platforms and unbundled executables retain the launch directory.
void initialize_bundle_runtime();
std::string resolve_launch_path(const std::string &path);
} // namespace hesiod
