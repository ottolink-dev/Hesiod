param(
    [Parameter(Mandatory)][string]$Source,
    [Parameter(Mandatory)][string]$Output,
    [Parameter(Mandatory)][string]$Dependencies
)
$ErrorActionPreference = 'Stop'
$stage = Join-Path $env:RUNNER_TEMP 'hesiod-windows'
New-Item -ItemType Directory -Force $stage, $Output | Out-Null
Copy-Item "$Source/build/bin/hesiod.exe" $stage
Copy-Item "$Source/LICENSE" $stage
Copy-Item "$Source/build/bin/data" $stage -Recurse
# Include both CRT and OpenMP runtimes; Qt deployment alone may omit vcomp140.
foreach ($runtime in @('Microsoft.VC143.CRT', 'Microsoft.VC143.OpenMP')) {
    $runtimeDir = Join-Path $env:VCToolsRedistDir "x64/$runtime"
    if (-not (Test-Path $runtimeDir)) { throw "Missing MSVC runtime: $runtimeDir" }
    Copy-Item "$runtimeDir/*.dll" $stage
}
# vcpkg supplies non-Qt runtime dependencies (including their transitive DLLs).
Copy-Item "$Dependencies/*.dll" $stage
& "$env:QT_ROOT_DIR/bin/windeployqt.exe" --release --compiler-runtime --no-translations "$stage/hesiod.exe"
if ($LASTEXITCODE) { throw "windeployqt failed: $LASTEXITCODE" }
if (-not (Test-Path "$stage/platforms/qwindows.dll")) { throw 'Qt platform plugin missing' }
if (-not (Test-Path "$stage/QtWebEngineProcess.exe")) { throw 'WebEngine helper missing' }
Set-Content -Path "$stage/SOURCE.txt" -Value $env:SOURCE_SHA
# The application initializes OpenCL before parsing --help. Hosted Windows
# runners have no GPU driver, so validate native dependency loading instead.
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class HesiodNativeCheck {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibraryExW(string name, IntPtr file, uint flags);
    [DllImport("kernel32.dll")]
    public static extern bool FreeLibrary(IntPtr handle);
}
'@
$imports = @()
foreach ($exe in @("$stage/hesiod.exe", "$stage/QtWebEngineProcess.exe")) {
    $dump = & dumpbin /dependents $exe
    if ($LASTEXITCODE) { throw "Cannot inspect $exe" }
    foreach ($line in $dump) {
        if ($line -match '^\s+([\w.-]+\.dll)\s*$') { $imports += $Matches[1] }
    }
}
if (-not $imports.Count) { throw 'No executable imports found' }
$originalPath = $env:PATH
try {
    $env:PATH = "$stage;$env:SystemRoot/System32;$env:SystemRoot"
    $libraries = @($imports | Sort-Object -Unique)
    $libraries += @(Get-ChildItem $stage -Filter '*.dll' -Recurse | ForEach-Object FullName)
    foreach ($library in $libraries) {
        $handle = [HesiodNativeCheck]::LoadLibraryExW($library, [IntPtr]::Zero, 0)
        if ($handle -eq [IntPtr]::Zero) {
            $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
            throw "Cannot load packaged dependency $library (Windows error $code)"
        }
        [HesiodNativeCheck]::FreeLibrary($handle) | Out-Null
    }
} finally { $env:PATH = $originalPath }
Compress-Archive -Path "$stage/*" -DestinationPath "$Output/hesiod-windows-x86_64.zip" -Force
