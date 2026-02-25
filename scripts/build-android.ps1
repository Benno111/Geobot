param(
    [ValidateSet("Android32", "Android64", "Both")]
    [string]$Target = "Both",
    [string]$Config = "Release",
    [string]$Ndk = $env:ANDROID_NDK_ROOT,
    [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount),
    [switch]$Ninja,
    [switch]$BuildOnly,
    [switch]$UseCompilerCache = $true,
    [switch]$Fast,
    [switch]$EnableHighPerformanceGpu,
    [switch]$UsePrebuilt
)

$ErrorActionPreference = "Stop"

if ($Fast) {
    $Ninja = $true
    $BuildOnly = $true
}

if ($UsePrebuilt) {
    $BuildOnly = $true
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

if ($Target -eq "Android32" -or $Target -eq "Both") {
    Invoke-AndroidBuild -Platform "android32" -Config $Config -Ndk $Ndk -Jobs $Jobs -UseNinja:$Ninja -BuildOnly:$BuildOnly -CompilerLauncher $compilerLauncher
}

if ($Target -eq "Android64" -or $Target -eq "Both") {
    Invoke-AndroidBuild -Platform "android64" -Config $Config -Ndk $Ndk -Jobs $Jobs -UseNinja:$Ninja -BuildOnly:$BuildOnly -CompilerLauncher $compilerLauncher
}

Write-Host "Android build completed."
