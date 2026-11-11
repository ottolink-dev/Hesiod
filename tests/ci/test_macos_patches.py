"""Check dependency patching across the November OpenMP change and reruns."""
from pathlib import Path
import json
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
LEGACY = 'add_compile_options(${OpenMP_CXX_FLAGS})'
MODERN = 'add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${OpenMP_CXX_FLAGS}>)'
CMAKE = '''find_package(GSL REQUIRED COMPONENTS gsl gslcblas)
find_package(assimp REQUIRED)
find_package(glm REQUIRED)
find_package(OpenMP REQUIRED)
find_package(OpenCV REQUIRED)
find_package(OpenCL REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(TIFF REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(zstd REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

# OpenMP
{openmp}

# OpenCL
add_compile_definitions(CL_HPP_MINIMUM_OPENCL_VERSION=120
                        CL_HPP_TARGET_OPENCL_VERSION=120)
'''


class DependencyPatchTests(unittest.TestCase):
    def test_legacy_and_modern_pins_are_repeatable(self):
        for flags in (LEGACY, MODERN):
            with self.subTest(flags=flags), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                highmap = root / 'external/HighMap'
                highmap.mkdir(parents=True)
                # Give git apply a repository boundary independent of the host.
                subprocess.run(['git', 'init', '-q', str(highmap)], check=True)
                (highmap / 'CMakeLists.txt').write_text(CMAKE.replace('{openmp}', flags))
                kernel = highmap / 'HighMap/src/gpu_opencl/kernels/common/_common_rand.cl'
                kernel.parent.mkdir(parents=True)
                kernel.write_text('float hash13f(float3 p);\n')
                meta = root / 'external/Meta/Meta/include/meta/type/attribute_traits_std.inl'
                meta.parent.mkdir(parents=True)
                meta.write_text(' * @copyright Copyright (c) 2026\n */\n#pragma once\n'
                                '#include <string>\n#include <vector>\n\n'
                                '  }\n};\n\n} // namespace meta')
                subprocess.run(['git', 'init', '-q', str(root / 'external/Meta')], check=True)
                shutil.copytree(ROOT / 'scripts/macos', root / 'scripts/macos')
                command = ['bash', '-euc', 'source_dir="$1"; source "$1/scripts/macos/patch_dependencies.sh"',
                           'patch-test', str(root)]
                subprocess.run(command, check=True, capture_output=True)
                patched = (highmap / 'CMakeLists.txt').read_text()
                subprocess.run(command, check=True, capture_output=True)
                self.assertEqual((highmap / 'CMakeLists.txt').read_text(), patched)
                self.assertIn('separate_arguments(HIGHMAP_OPENMP_FLAGS', patched)
                self.assertIn('find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)', patched)
                if flags == MODERN:
                    self.assertIn('"$<$<COMPILE_LANGUAGE:CXX>:${HIGHMAP_OPENMP_FLAGS}>"', patched)
                    if shutil.which('cmake') and shutil.which('ninja'):
                        self.check_language_flags(root, patched)

    def check_language_flags(self, root, patched):
        project = root / 'flags'
        project.mkdir()
        options = patched.split('# OpenMP\n')[1].split('# OpenCL')[0]
        (project / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.22)\nproject(flags LANGUAGES C CXX)\n'
            'set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")\n' + options +
            'add_library(flags OBJECT sample.c sample.cpp)\n')
        for name in ('sample.c', 'sample.cpp'):
            (project / name).write_text('int sample(void) { return 0; }\n')
        subprocess.run(['cmake', '-S', str(project), '-B', str(project / 'build'),
                        '-G', 'Ninja', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'],
                       check=True, capture_output=True)
        commands = json.loads((project / 'build/compile_commands.json').read_text())
        for entry in commands:
            args = shlex.split(entry['command'])
            if entry['file'].endswith('.cpp'):
                self.assertIn('-Xpreprocessor', args)
                self.assertIn('-fopenmp', args)
                self.assertNotIn('-Xpreprocessor -fopenmp', args)
            else:
                self.assertNotIn('-fopenmp', args)



if __name__ == '__main__':
    unittest.main()
