"""Exercise the package audit against real Mach-O dependency layouts."""
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts/macos'))
from verify_bundle import audit
from package_app import normalize_runpaths


@unittest.skipUnless(sys.platform == 'darwin', 'requires Apple linker tools')
class BundleAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='hesiod-bundle-test-')
        cls.root = Path(cls.temp.name)
        cls.fixture = cls.root / 'Fixture.app'
        cls.libdir = cls.fixture / 'Contents/Frameworks'
        cls.macdir = cls.fixture / 'Contents/MacOS'
        cls.libdir.mkdir(parents=True)
        cls.macdir.mkdir()
        (cls.root / 'terrain.c').write_text('int terrain(void) { return 0; }\n')
        (cls.root / 'main.c').write_text('int terrain(void); int main(void) { return terrain(); }\n')
        subprocess.run(['clang', '-arch', 'arm64', '-dynamiclib', str(cls.root / 'terrain.c'),
                        '-Wl,-install_name,@rpath/libterrain.dylib', '-o',
                        str(cls.libdir / 'libterrain.dylib')], check=True)
        subprocess.run(['clang', '-arch', 'arm64', str(cls.root / 'main.c'),
                        '-L' + str(cls.libdir), '-lterrain',
                        '-Wl,-rpath,@executable_path/../Frameworks',
                        '-Wl,-headerpad_max_install_names', '-o',
                        str(cls.macdir / 'Fixture')], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.work = tempfile.TemporaryDirectory(dir=self.root)
        self.addCleanup(self.work.cleanup)
        self.app = Path(self.work.name) / 'Test.app'
        shutil.copytree(self.fixture, self.app)

    def test_self_contained_dependency_tree_passes(self):
        report = audit(self.app)
        self.assertEqual(report['mach_o_files'], 2)
        self.assertEqual(report['external_dependencies'], [])

    def test_external_dependency_in_plugin_is_rejected(self):
        plugin = self.app / 'Contents/PlugIns/terrain.dylib'
        plugin.parent.mkdir()
        shutil.copy2(self.libdir / 'libterrain.dylib', plugin)
        subprocess.run(['install_name_tool', '-change', '/usr/lib/libSystem.B.dylib',
                        '/opt/homebrew/lib/libMissing.dylib', str(plugin)], check=True)
        with self.assertRaisesRegex(RuntimeError, 'unbundled dependency'):
            audit(self.app)

    def test_missing_rpath_dependency_is_rejected(self):
        (self.app / 'Contents/Frameworks/libterrain.dylib').unlink()
        with self.assertRaisesRegex(RuntimeError, 'unbundled dependency'):
            audit(self.app)

    def test_development_rpath_is_rejected(self):
        subprocess.run(['install_name_tool', '-add_rpath', '/opt/homebrew/lib',
                        str(self.app / 'Contents/MacOS/Fixture')], check=True)
        with self.assertRaisesRegex(RuntimeError, 'external rpath'):
            audit(self.app)

    def test_missing_loader_search_path_is_rejected(self):
        subprocess.run(['install_name_tool', '-delete_rpath',
                        '@executable_path/../Frameworks',
                        str(self.app / 'Contents/MacOS/Fixture')], check=True)
        with self.assertRaisesRegex(RuntimeError, 'unbundled dependency'):
            audit(self.app)

    def test_helper_imports_are_rewritten_to_bundled_libraries(self):
        helper = self.app / 'Contents/Frameworks/Helpers.app/Contents/MacOS/Helper'
        helper.parent.mkdir(parents=True)
        shutil.copy2(self.macdir / 'Fixture', helper)
        subprocess.run(['install_name_tool', '-change', '@rpath/libterrain.dylib',
                        '/opt/homebrew/lib/libterrain.dylib', str(helper)], check=True)
        normalize_runpaths(self.app)
        self.assertEqual(audit(self.app)['mach_o_files'], 3)

    def test_escaping_resource_symlink_is_rejected(self):
        (self.app / 'Contents/Resources').symlink_to(self.root)
        with self.assertRaisesRegex(RuntimeError, 'Invalid symlink'):
            audit(self.app)


if __name__ == '__main__':
    unittest.main()
