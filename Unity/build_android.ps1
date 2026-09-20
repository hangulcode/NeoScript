# build_android.ps1 — Android 네이티브 플러그인을 빌드해 패키지에 넣는다.
#
#   pwsh Unity\build_android.ps1                 # arm64-v8a
#   pwsh Unity\build_android.ps1 -Abi armeabi-v7a
#
# NDK 는 ANDROID_NDK_ROOT / ANDROID_NDK_HOME / ANDROID_HOME\ndk\* / %LOCALAPPDATA%\Android\Sdk\ndk\*
# 순서로 찾는다. 빌드 도구는 ninja 가 PATH 에 있으면 그걸 쓰고, 없으면 NDK 가 같이 배포하는
# GNU make 로 떨어진다 (별도 설치 없이 돌게 하려는 것이다).

[CmdletBinding()]
param(
    [ValidateSet('arm64-v8a', 'armeabi-v7a', 'x86_64')]
    [string]$Abi = 'arm64-v8a',
    [int]$ApiLevel = 24,
    [string]$NdkRoot = '',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

function Find-Ndk {
    param([string]$Explicit)

    if ($Explicit) { return $Explicit }
    foreach ($name in 'ANDROID_NDK_ROOT', 'ANDROID_NDK_HOME') {
        $value = [Environment]::GetEnvironmentVariable($name)
        if ($value -and (Test-Path $value)) { return $value }
    }
    $roots = @()
    if ($env:ANDROID_HOME) { $roots += (Join-Path $env:ANDROID_HOME 'ndk') }
    $roots += (Join-Path $env:LOCALAPPDATA 'Android\Sdk\ndk')
    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }
        # 여러 버전이 깔려 있으면 가장 높은 것을 쓴다.
        $latest = Get-ChildItem $root -Directory | Sort-Object Name -Descending | Select-Object -First 1
        if ($latest) { return $latest.FullName }
    }
    throw "Android NDK not found. Set ANDROID_NDK_ROOT or pass -NdkRoot."
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$ndk = Find-Ndk -Explicit $NdkRoot
$toolchain = Join-Path $ndk 'build\cmake\android.toolchain.cmake'
if (-not (Test-Path $toolchain)) { throw "NDK toolchain file missing: $toolchain" }

$buildDir = Join-Path $repoRoot "build\unity-android-$Abi"
$pluginDir = Join-Path $PSScriptRoot "com.neoscript.unity\Runtime\Plugins\Android\$Abi"

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}

# 생성기 고르기: ninja 가 있으면 ninja, 없으면 NDK 의 make.
$generator = 'Ninja'
$makeProgram = $null
$ninja = Get-Command ninja -ErrorAction SilentlyContinue
if ($ninja) {
    $makeProgram = $ninja.Source
} else {
    $ndkMake = Join-Path $ndk 'prebuilt\windows-x86_64\bin\make.exe'
    if (-not (Test-Path $ndkMake)) { throw "neither ninja nor the NDK's make was found" }
    $generator = 'Unix Makefiles'
    $makeProgram = $ndkMake
}

Write-Host "==> NDK       $ndk" -ForegroundColor Cyan
Write-Host "==> generator $generator ($makeProgram)" -ForegroundColor Cyan

& cmake -S $repoRoot -B $buildDir -G $generator `
    "-DCMAKE_MAKE_PROGRAM=$makeProgram" `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DANDROID_ABI=$Abi" `
    "-DANDROID_PLATFORM=android-$ApiLevel" `
    '-DCMAKE_BUILD_TYPE=Release' `
    '-DNEOSCRIPT_BUILD_SHARED=ON' `
    '-DNEOSCRIPT_BUILD_SMOKE=OFF'
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($LASTEXITCODE)" }

& cmake --build $buildDir --target NeoScriptShared --parallel
if ($LASTEXITCODE -ne 0) { throw "cmake build failed ($LASTEXITCODE)" }

$so = Join-Path $buildDir 'libNeoScript.so'
if (-not (Test-Path $so)) { throw "libNeoScript.so not found under $buildDir" }

New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
Copy-Item $so (Join-Path $pluginDir 'libNeoScript.so') -Force

$info = Get-Item (Join-Path $pluginDir 'libNeoScript.so')
Write-Host "==> deployed $($info.FullName) ($([math]::Round($info.Length/1KB)) KB)" -ForegroundColor Green
