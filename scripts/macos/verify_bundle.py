#!/usr/bin/env python3
"""Audit every Mach-O in a bundle, including plugins and WebEngine helpers."""
import argparse
import json
import plistlib
import re
import subprocess
from pathlib import Path

MAGIC = {bytes.fromhex(s) for s in (
    'feedface', 'cefaedfe', 'feedfacf', 'cffaedfe', 'cafebabe', 'bebafeca',
    'cafebabf', 'bfbafeca')}
SYSTEM = ('/System/Library/', '/usr/lib/')


def run(*args):
    return subprocess.check_output([str(a) for a in args], text=True)


def binaries(app):
    result = set()
    for path in app.rglob('*'):
        if path.is_file():
            with path.open('rb') as stream:
                if stream.read(4) in MAGIC:
                    result.add(path.resolve())
    return sorted(result)


def metadata(binary):
    text = run('otool', '-l', binary)
    dependencies, rpaths, versions = [], [], []
    for block in text.split('Load command ')[1:]:
        if re.search(r'cmd LC_(?:LOAD_DYLIB|LOAD_WEAK_DYLIB|REEXPORT_DYLIB|LOAD_UPWARD_DYLIB)\b', block):
            dependencies.append(re.search(r'\n\s+name (.*?) \(offset', block)[1])
        elif 'cmd LC_RPATH\n' in block:
            rpaths.append(re.search(r'\n\s+path (.*?) \(offset', block)[1])
        elif 'cmd LC_BUILD_VERSION\n' in block:
            versions.extend(re.findall(r'\n\s+minos ([\d.]+)', block))
        elif 'cmd LC_VERSION_MIN_MACOSX\n' in block:
            versions.extend(re.findall(r'\n\s+version ([\d.]+)', block))
    return dependencies, rpaths, versions


def executable_dir(binary, app):
    for parent in binary.parents:
        if parent.suffix == '.app':
            return parent / 'Contents/MacOS'
    return app / 'Contents/MacOS'


def expand(path, binary, app):
    return Path(path.replace('@loader_path', str(binary.parent))
                .replace('@executable_path', str(executable_dir(binary, app))))


def audit(app):
    app = Path(app).resolve()
    files = binaries(app)
    if not files:
        raise RuntimeError('Bundle contains no Mach-O executables')
    versions, errors = [], []
    details = {binary: metadata(binary) for binary in files}
    entrypoints = {}
    for binary in files:
        if binary.parent.name == 'MacOS':
            entrypoints.setdefault(binary.parent, []).append(binary)
    for binary in files:
        subprocess.run(['lipo', str(binary), '-verify_arch', 'arm64'], check=True)
        dependencies, rpaths, minimums = details[binary]
        versions.extend(minimums)
        for path in rpaths:
            if path.startswith(SYSTEM):
                continue
            if path.startswith('/') and not Path(path).resolve().is_relative_to(app):
                errors.append(f'{binary.relative_to(app)}: external rpath {path}')
        for dep in dependencies:
            if dep.startswith(SYSTEM):
                continue
            if dep.startswith('@rpath/'):
                search = [expand(p, binary, app) for p in rpaths if '@rpath' not in p]
                # dyld inherits runpaths from the executable. Do not assume a
                # Frameworks directory is searchable just because it exists.
                for entry in entrypoints.get(executable_dir(binary, app), []):
                    search += [expand(p, entry, app) for p in details[entry][1]
                               if '@rpath' not in p]
                candidates = [p / dep[len('@rpath/'):] for p in search]
            else:
                candidates = [expand(dep, binary, app)]
            if not any(p.exists() and p.resolve().is_relative_to(app) for p in candidates):
                errors.append(f'{binary.relative_to(app)}: unbundled dependency {dep}')
    # Catch dangling or escaping resource/framework symlinks as well.
    for path in app.rglob('*'):
        if path.is_symlink() and (not path.exists() or not path.resolve().is_relative_to(app)):
            errors.append(f'Invalid symlink: {path.relative_to(app)}')
    if errors:
        raise RuntimeError('\n'.join(errors))
    minimum = max(versions, key=lambda v: tuple(map(int, v.split('.'))))
    return {'architecture': 'arm64', 'mach_o_files': len(files),
            'minimum_macos': minimum, 'external_dependencies': []}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    report = audit(args.app)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(args.app)], check=True)
    with (args.app / 'Contents/Info.plist').open('rb') as stream:
        info = plistlib.load(stream)
    if info.get('LSMinimumSystemVersion') != report['minimum_macos']:
        raise RuntimeError('Info.plist does not declare the dependencies\' minimum macOS version')
    print(json.dumps(report, indent=2))
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
