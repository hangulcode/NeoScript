# build_windows.ps1 — Windows x64 네이티브 플러그인을 빌드해 패키지에 넣는다.
#
#   pwsh Unity\build_windows.ps1
#
# 에디터가 NeoScript.dll 을 물고 있으면 복사가 실패한다. 그럴 때는 에디터에서
# Tools > NeoScript > Unload native plugin 을 한 번 누르거나 에디터를 껐다 켤 것.

[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [string]$Generator = '',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build\unity-win64'
$pluginDir = Join-Path $PSScriptRoot 'com.neoscript.unity\Runtime\Plugins\x86_64'

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}

$configureArgs = @('-S', $repoRoot, '-B', $buildDir, '-DNEOSCRIPT_BUILD_SHARED=ON', '-DNEOSCRIPT_BUILD_SMOKE=OFF')
if ($Generator) {
    $configureArgs += @('-G', $Generator, '-A', 'x64')
} else {
    # 기본 생성기는 설치된 최신 Visual Studio 다. CMake 가 알아서 고르게 둔다.
    $configureArgs += @('-A', 'x64')
}

Write-Host "==> configure" -ForegroundColor Cyan
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($LASTEXITCODE)" }

Write-Host "==> build ($Configuration)" -ForegroundColor Cyan
& cmake --build $buildDir --config $Configuration --target NeoScriptShared
if ($LASTEXITCODE -ne 0) { throw "cmake build failed ($LASTEXITCODE)" }

$candidates = @(
    (Join-Path $buildDir "$Configuration\NeoScript.dll"),
    (Join-Path $buildDir 'NeoScript.dll')
)
$dll = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $dll) { throw "NeoScript.dll not found under $buildDir" }

New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
Copy-Item $dll (Join-Path $pluginDir 'NeoScript.dll') -Force

$info = Get-Item (Join-Path $pluginDir 'NeoScript.dll')
Write-Host "==> deployed $($info.FullName) ($([math]::Round($info.Length/1KB)) KB)" -ForegroundColor Green
