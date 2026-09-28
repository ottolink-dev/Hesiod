#!@shell@
# Launcher for the Nix package of Hesiod.
#
# Hesiod resolves its `data/` directory (icons, node docs, default project,
# skybox, ...) relative to the current working directory, and it also writes a
# few files there at run time (OpenCV build log, texture-downloader cache,
# batch-mode outputs). The Nix store is read-only, so run from a per-user state
# directory that carries a symlink to the packaged data.
set -eu

state="${XDG_DATA_HOME:-$HOME/.local/share}/hesiod"
mkdir -p "$state"
ln -sfn "@out@/share/hesiod/data" "$state/data"

# OpenCL ICD discovery: NixOS installs vendor ICDs under /run/opengl-driver.
# Other distros use the ocl-icd default (/etc/OpenCL/vendors); leave those
# alone, and never override an explicit choice.
if [ -z "${OCL_ICD_VENDORS:-}" ] && [ -d /run/opengl-driver/etc/OpenCL/vendors ]; then
  export OCL_ICD_VENDORS=/run/opengl-driver/etc/OpenCL/vendors
fi

# Relative paths on the command line (a .hsd to open, --file=..., --batch=...)
# must survive the chdir below: absolutise anything that names an existing
# path, both bare and in --flag=value form.
abs() { case $1 in /*) printf '%s' "$1" ;; *) printf '%s' "$PWD/$1" ;; esac; }
i=0
n=$#
while [ "$i" -lt "$n" ]; do
  a=$1
  shift
  case $a in
    --*=*)
      v=${a#*=}
      if [ -e "$v" ]; then a="${a%%=*}=$(abs "$v")"; fi
      ;;
    *)
      if [ -e "$a" ]; then a=$(abs "$a"); fi
      ;;
  esac
  set -- "$@" "$a"
  i=$((i + 1))
done

cd "$state"
exec "@out@/libexec/hesiod/hesiod" "$@"
