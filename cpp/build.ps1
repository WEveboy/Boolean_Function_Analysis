$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer (vswhere.exe) was not found.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio 2022 C++ build tools were not found.' }
$vs = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vs)) { throw 'vcvars64.bat was not found.' }
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here 'build'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$cmdline = '"' + $vs + '" && cl /nologo /std:c++20 /O2 /EHsc /MT /utf-8 /W4 /Fo:"' + (Join-Path $out 'bf-core.obj') + '" /Fe:"' + (Join-Path $out 'bf-core.exe') + '" "' + (Join-Path $here 'bf_core.cpp') + '"'
cmd.exe /c $cmdline
if ($LASTEXITCODE -ne 0) { throw "C++ compilation failed with exit code $LASTEXITCODE" }
