#!/bin/bash

echo "- json"

jq . Hesiod/data/node_documentation.json --indent 4 > tmp.json
mv tmp.json Hesiod/data/node_documentation.json

echo "- python"

DIRS="bridges/Blender/streamer_addon"

for D in ${DIRS}; do
    for F in `find ${D}/. -type f \( -iname \*.py \)`; do
	echo ${F}
	yapf -i ${F}
    done
done
	
echo "- clang-format"

# directories to be formatted (recursive search)
DIRS="Hesiod/include Hesiod/src Hesiod/app"
FORMAT_CMD="clang-format -style=file:scripts/clang_style -i"

for D in ${DIRS}; do
    for F in `find ${D}/. -type f \( -iname \*.hpp -o -iname \*.cpp \)`; do
	echo ${F}
	${FORMAT_CMD} ${F}
    done
done

echo "- cmake-format"

cmake-format -i CMakeLists.txt
cmake-format -i Hesiod/CMakeLists.txt
cmake-format -i cmake/HesiodCompilerOptions.cmake
cmake-format -i cmake/HesiodPlatform.cmake
cmake-format -i cmake/HesiodQtLoggingRules.cmake
