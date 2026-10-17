#!/usr/bin/env python3
"""Create a standalone ARM64 app and ZIP with ad-hoc signing (no Apple account)."""
import argparse
import hashlib
import json
import os
import plistlib
import shutil
import subprocess
import tempfile
from pathlib import Path

from verify_bundle import audit, binaries, metadata, run

ROOT = Path(__file__).resolve().parents[2]


def provenance(build, resources, plugin_sources):
    lines = ['Hesiod source: https://github.com/ottolink-dev/Hesiod',
             run('git', '-C', ROOT, 'rev-parse', 'HEAD').strip(),
             run('git', '-C', ROOT, 'describe', '--always', '--dirty').strip(),
             'Pinned submodules:',
             run('git', '-C', ROOT, 'submodule', 'status', '--recursive').strip(),
             'Dependency patches: scripts/macos/patches in the source repository.',
             'FetchContent source revisions:']
    licenses = resources / 'licenses'
    licenses.mkdir(exist_ok=True)
    module_status = run('git', '-C', ROOT, 'submodule', 'status', '--recursive')
    roots = [ROOT, *(ROOT / line.split()[1] for line in module_status.splitlines()),
             *sorted((build / '_deps').glob('*-src'))]
    for source in roots:
        if not source.is_dir():
            continue
        if source.parent.name == '_deps' and (source / '.git').exists():
            lines.append(f'{source.name}: {run("git", "-C", source, "rev-parse", "HEAD").strip()}')
        for entry in source.iterdir():
            if entry.is_file() and entry.name.lower().startswith(('license', 'copying', 'notice', 'third_party')):
                dest = licenses / source.name / entry.name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(entry, dest)
    # Keep exact Homebrew source/patch/build recipes and available license texts
    # for the libraries that this build deploys, including their dependencies.
    brew = Path(run('brew', '--prefix').strip())
    packages = set()
    pending = [build / 'bin/Hesiod.app/Contents/MacOS/Hesiod']
    pending += plugin_sources
    seen = set()
    while pending:
        binary = pending.pop().resolve()
        if binary in seen or not binary.exists():
            continue
        seen.add(binary)
        if binary.is_relative_to(brew / 'Cellar'):
            parts = binary.relative_to(brew / 'Cellar').parts
            if len(parts) >= 3:
                packages.add(brew / 'Cellar' / parts[0] / parts[1])
        for dep in metadata(binary)[0]:
            if dep.startswith(str(brew)):
                pending.append(Path(dep))
            elif dep.startswith('@rpath/') and (brew / 'lib' / dep[7:]).exists():
                pending.append(brew / 'lib' / dep[7:])
    lines.append('Homebrew runtime source recipes and licenses:')
    for package in sorted(packages):
        lines.append(str(package.relative_to(brew / 'Cellar')))
        dest = licenses / 'homebrew' / package.parent.name
        dest.mkdir(parents=True, exist_ok=True)
        for entry in package.iterdir():
            if entry.is_file() and entry.name.lower().startswith(('license', 'copying', 'notice')):
                shutil.copy2(entry, dest / entry.name)
            elif entry.is_dir() and entry.name.upper() == 'LICENSES':
                shutil.copytree(entry, dest / entry.name, dirs_exist_ok=True)
        for recipe in (package / '.brew').glob('*.rb'):
            shutil.copy2(recipe, dest / recipe.name)
    (resources / 'SOURCE.txt').write_text('\n'.join(lines) + '\n')
    shutil.copy2(ROOT / 'LICENSE', resources / 'LICENSE')
    shutil.copytree(ROOT / 'scripts/macos/patches', resources / 'dependency-patches', dirs_exist_ok=True)
    working_tree_sources(resources)


def working_tree_sources(resources):
    # Development packages must retain changes that are not yet at the source
    # revision above. Include the patch plus the newly added source files.
    (resources / 'source-changes.patch').write_text(run('git', '-C', ROOT, 'diff', '--binary', 'HEAD'))
    untracked = subprocess.check_output(
        ['git', '-C', str(ROOT), 'ls-files', '--others', '--exclude-standard', '-z'])
    for name in untracked.decode().split('\0'):
        if name:
            source = ROOT / name
            if source.is_file():
                dest = resources / 'source-additions' / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, dest)


def icon(app):
    with tempfile.TemporaryDirectory(prefix='hesiod-icon-') as temp:
        iconset = Path(temp) / 'Hesiod.iconset'
        iconset.mkdir()
        for size in (16, 32, 128, 256, 512):
            for scale in (1, 2):
                name = f'icon_{size}x{size}' + ('@2x' if scale == 2 else '') + '.png'
                subprocess.run(['sips', '-z', str(size * scale), str(size * scale),
                                str(ROOT / 'Hesiod/data/hesiod_icon.png'), '--out', str(iconset / name)],
                               check=True, stdout=subprocess.DEVNULL)
        subprocess.run(['iconutil', '-c', 'icns', str(iconset), '-o',
                        str(app / 'Contents/Resources/Hesiod.icns')], check=True)


def sign(app):
    # Sign nested code from the inside out; --deep is used for verification only.
    for binary in binaries(app):
        subprocess.run(['codesign', '--force', '--sign', '-', str(binary)],
                       check=True, stdout=subprocess.DEVNULL)
    bundles = [p for p in app.rglob('*') if p.is_dir() and not p.is_symlink()
               and p.suffix in ('.app', '.framework', '.xpc', '.bundle')]
    for bundle in sorted(bundles, key=lambda p: len(p.parts), reverse=True) + [app]:
        subprocess.run(['codesign', '--force', '--sign', '-', str(bundle)], check=True)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)


def normalize_runpaths(app):
    app = Path(app).resolve()
    frameworks = app / 'Contents/Frameworks'
    for binary in binaries(app):
        dependencies, rpaths, _ = metadata(binary)
        changes = []
        # Homebrew's split Qt packages can leave the embedded Chromium helper
        # with absolute Qt imports even after macdeployqt. Rewrite only imports
        # whose exact framework/dylib is already present in this bundle.
        for dependency in dependencies:
            if not dependency.startswith('/') or dependency.startswith(('/System/Library/', '/usr/lib/')):
                continue
            parts = Path(dependency).parts
            framework = next((i for i, p in enumerate(parts) if p.endswith('.framework')), None)
            target = (frameworks.joinpath(*parts[framework:]) if framework is not None
                      else frameworks / Path(dependency).name)
            if target.is_file():
                local = '@loader_path/' + os.path.relpath(target, binary.parent)
                changes += ['-change', dependency, local]
        for path in rpaths:
            if path.startswith('/') and not path.startswith(('/System/Library/', '/usr/lib/')):
                changes += ['-delete_rpath', path]
        if any(dep.startswith('@rpath/') for dep in dependencies):
            local = '@loader_path/' + os.path.relpath(frameworks, binary.parent)
            if local not in rpaths:
                changes += ['-add_rpath', local]
        if changes:
            subprocess.run(['install_name_tool', *changes, str(binary)], check=True)


def package(build, output, qt):
    build, output, qt = build.resolve(), output.resolve(), qt.resolve()
    app = output / 'Hesiod.app'
    if app.exists():
        raise RuntimeError(f'{app} already exists; choose a fresh --output directory')
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run(['ditto', str(build / 'bin/Hesiod.app'), str(app)], check=True)
    resources = app / 'Contents/Resources'
    icon(app)
    # Avoid optional SQL and virtual-keyboard plugins: this is a Widgets app.
    # Explicit plugins are treated as executables by macdeployqt so their full
    # library dependency trees are deployed and rewritten too.
    plugins, plugin_sources = [], []
    plugin_dir = Path(run(qt / 'bin/qtpaths', '--query', 'QT_INSTALL_PLUGINS').strip())
    library_dir = Path(run(qt / 'bin/qtpaths', '--query', 'QT_INSTALL_LIBS').strip())
    for group in ('platforms', 'imageformats', 'iconengines', 'styles', 'tls', 'networkinformation'):
        source = plugin_dir / group
        dest = app / 'Contents/PlugIns' / group
        for plugin in sorted(source.glob('*.dylib')):
            if group == 'platforms' and plugin.name != 'libqcocoa.dylib':
                continue
            dest.mkdir(parents=True, exist_ok=True)
            shutil.copy2(plugin, dest / plugin.name)
            plugins.append(dest / plugin.name)
            plugin_sources.append(plugin)
    if not any(p.name == 'libqcocoa.dylib' for p in plugins):
        raise RuntimeError('Qt Cocoa platform plugin is missing')
    provenance(build, resources, plugin_sources)
    command = [str(qt / 'bin/macdeployqt'), str(app), '-no-plugins', '-no-codesign',
               '-verbose=2', f'-libpath={library_dir}']
    command += [f'-executable={p}' for p in plugins]
    with (output / 'deployment.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode or 'ERROR:' in (output / 'deployment.log').read_text():
        raise RuntimeError(f'Qt deployment failed; see {output / "deployment.log"}')

    finalize(app, output)


def finalize(app, output):
    resources = app / 'Contents/Resources'
    normalize_runpaths(app)
    report = audit(app)
    (resources / 'qt.conf').write_text(
        '[Paths]\nPlugins = PlugIns\nLibraries = Frameworks\nTranslations = Resources/translations\n')
    working_tree_sources(resources)
    info_path = app / 'Contents/Info.plist'
    with info_path.open('rb') as stream:
        info = plistlib.load(stream)
    info.update(CFBundleIconFile='Hesiod.icns', NSHighResolutionCapable=True,
                LSMinimumSystemVersion=report['minimum_macos'])
    with info_path.open('wb') as stream:
        plistlib.dump(info, stream)
    sign(app)
    report.update(signing='ad-hoc', notarized=False)
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    archive = output / 'hesiod-macos-arm64.zip'
    subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', str(app), str(archive)], check=True)
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    (output / (archive.name + '.sha256')).write_text(f'{digest}  {archive.name}\n')
    shutil.copy2(resources / 'SOURCE.txt', output / 'SOURCE.txt')
    print(json.dumps(report, indent=2))
    print(f'App: {app}\nArchive: {archive}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/macos')
    parser.add_argument('--output', type=Path, default=ROOT / 'output/macos-package')
    parser.add_argument('--qt', type=Path, default=Path(run('brew', '--prefix', 'qt').strip()))
    args = parser.parse_args()
    package(args.build, args.output, args.qt)
