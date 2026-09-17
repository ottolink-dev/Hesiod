#!/bin/sh

# Due to not being able to change the data dir, we have to get a bit creative
export HESIOD_TMP_DIR=$(mktemp -d)
cd $HESIOD_TMP_DIR

mkdir -p $XDG_DATA_HOME/hesiod
mkdir -p $XDG_DATA_HOME/hesiod/texture_downloader

ln -s /app/hesiod/data data
ln -s /app/hesiod/hesiod hesiod
ln -s $XDG_DATA_HOME/hesiod/texture_downloader texture_downloader
ln -s $XDG_DATA_HOME/hesiod/imgui.ini imgui.ini
ln -s $XDG_DATA_HOME/hesiod/opencv_build_information.log opencv_build_information.log

export QTWEBENGINEPROCESS_PATH=/app/bin/QtWebEngineProcess

LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libOpenCL.so.1 ./hesiod $@

unlink opencv_build_information.log
unlink imgui.ini
unlink texture_downloader
unlink hesiod
unlink data

rmdir $HESIOD_TMP_DIR
