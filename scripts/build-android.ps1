param(
    [ValidateSet("Android32", "Android64", "Both")]
    [string]$Target = "Both",
    [string]$Config = "Release",
    [string]$Ndk = $env:ANDROID_NDK_ROOT,
    [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount),
    [switch]$Ninja,
    [switch]$BuildOnly,
    [switch]$SkipSdkSync,
    [switch]$SkipBinarySync,
    [switch]$UseCompilerCache = $true,
    [switch]$Fast,
    [switch]$EnableHighPerformanceGpu
)

$ErrorActionPreference = "Stop"

if ($Fast) {
    $Ninja = $true
    $BuildOnly = $true
    $SkipSdkSync = $true
    $SkipBinarySync = $true
}

if ($EnableHighPerformanceGpu) {
    Set-HighPerformanceGpuPreference -ToolNames @("geode", "cmake", "ninja")
    Write-Host "Applied Windows high-performance GPU preference for build tools (tool support dependent)."
}

function Get-CompilerLauncher {
    if (-not $UseCompilerCache) {
        return $null
    }

    if (Get-Command sccache -ErrorAction SilentlyContinue) {
        return "sccache"
    }
    if (Get-Command ccache -ErrorAction SilentlyContinue) {
        return "ccache"
    }

    return $null
}

function Set-HighPerformanceGpuPreference {
    param(
        [string[]]$ToolNames
    )

    if (-not $IsWindows) {
        return
    }

    $regPath = "HKCU:\Software\Microsoft\DirectX\UserGpuPreferences"
    if (-not (Test-Path $regPath)) {
        New-Item -Path $regPath -Force | Out-Null
    }

    foreach ($tool in $ToolNames) {
        $cmd = Get-Command $tool -ErrorAction SilentlyContinue
        if (-not $cmd -or [string]::IsNullOrWhiteSpace($cmd.Source)) {
            continue
        }

        # Windows per-app GPU preference: 2 = High performance GPU.
        New-ItemProperty -Path $regPath -Name $cmd.Source -PropertyType String -Value "GpuPreference=2;" -Force | Out-Null
    }
}

function Get-ModGeodeVersion {
    $modJsonPath = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "..")).Path "mod.json"
    if (-not (Test-Path $modJsonPath)) {
        throw "mod.json not found at $modJsonPath"
    }

    $modJson = Get-Content -Raw -Path $modJsonPath | ConvertFrom-Json
    $ver = [string]$modJson.geode
    if ([string]::IsNullOrWhiteSpace($ver)) {
        throw "mod.json does not define a valid geode version."
    }

    return $ver
}

function Ensure-SdkVersion {
    param(
        [string]$RequiredVersion
    )

    $sdkVerOutput = (& geode sdk version) 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to read Geode SDK version."
    }

    $sdkVerText = ($sdkVerOutput | Out-String)
    if ($sdkVerText -notmatch "Geode SDK version:\s*([0-9]+\.[0-9]+\.[0-9]+)") {
        throw "Could not parse Geode SDK version output: $sdkVerText"
    }

    $currentVersion = $Matches[1]
    if ($currentVersion -ne $RequiredVersion) {
        Write-Host "Updating Geode SDK from $currentVersion to $RequiredVersion..."
        & geode sdk update $RequiredVersion
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to update Geode SDK to $RequiredVersion."
        }
    }
}

function Ensure-AndroidBinaries {
    param(
        [string]$Platform,
        [string]$GeodeVersion
    )

    Write-Host "Ensuring Geode binaries for $Platform v$GeodeVersion..."
    & geode sdk install-binaries -p $Platform -v $GeodeVersion
    if ($LASTEXITCODE -ne 0) {
        throw "Failed installing Geode binaries for $Platform ($GeodeVersion)."
    }
}

function Invoke-AndroidBuild {
    param(
        [string]$Platform,
        [string]$Config,
        [string]$Ndk,
        [int]$Jobs,
        [bool]$UseNinja,
        [bool]$BuildOnly,
        [string]$CompilerLauncher
    )

    $cmd = @("build", "-p", $Platform, "--config", $Config)
    if ($UseNinja) {
        $cmd += "--ninja"
    }
    if ($BuildOnly) {
        $cmd += "--build-only"
    }
    if (-not [string]::IsNullOrWhiteSpace($Ndk)) {
        $cmd += @("--ndk", $Ndk)
    }
    if ($CompilerLauncher -and -not $BuildOnly) {
        $cmd += "--"
        $cmd += "-DCMAKE_C_COMPILER_LAUNCHER=$CompilerLauncher"
        $cmd += "-DCMAKE_CXX_COMPILER_LAUNCHER=$CompilerLauncher"
    }

    $previousParallelLevel = $env:CMAKE_BUILD_PARALLEL_LEVEL
    try {
        if ($Jobs -gt 0) {
            $env:CMAKE_BUILD_PARALLEL_LEVEL = [string]$Jobs
        }

        Write-Host "Running: geode $($cmd -join ' ') (CMAKE_BUILD_PARALLEL_LEVEL=$env:CMAKE_BUILD_PARALLEL_LEVEL)"
        & geode @cmd
        if ($LASTEXITCODE -ne 0) {
            throw "Android build failed for $Platform."
        }
    }
    finally {
        if ($null -eq $previousParallelLevel) {
            Remove-Item Env:\CMAKE_BUILD_PARALLEL_LEVEL -ErrorAction SilentlyContinue
        }
        else {
            $env:CMAKE_BUILD_PARALLEL_LEVEL = $previousParallelLevel
        }
    }
}

$compilerLauncher = Get-CompilerLauncher

if (-not $SkipSdkSync) {
    $geodeVersion = Get-ModGeodeVersion
    Ensure-SdkVersion -RequiredVersion $geodeVersion
}
else {
    $geodeVersion = Get-ModGeodeVersion
}

if (-not $SkipBinarySync) {
    if ($Target -eq "Both") {
        Ensure-AndroidBinaries -Platform "android" -GeodeVersion $geodeVersion
    }
}

if ($Target -eq "Android32" -or $Target -eq "Both") {
    if (-not $SkipBinarySync -and $Target -ne "Both") {
        Ensure-AndroidBinaries -Platform "android32" -GeodeVersion $geodeVersion
    }
    Invoke-AndroidBuild -Platform "android32" -Config $Config -Ndk $Ndk -Jobs $Jobs -UseNinja:$Ninja -BuildOnly:$BuildOnly -CompilerLauncher $compilerLauncher
}

if ($Target -eq "Android64" -or $Target -eq "Both") {
    if (-not $SkipBinarySync -and $Target -ne "Both") {
        Ensure-AndroidBinaries -Platform "android64" -GeodeVersion $geodeVersion
    }
    Invoke-AndroidBuild -Platform "android64" -Config $Config -Ndk $Ndk -Jobs $Jobs -UseNinja:$Ninja -BuildOnly:$BuildOnly -CompilerLauncher $compilerLauncher
}

Write-Host "Android build completed."
