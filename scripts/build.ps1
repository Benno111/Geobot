param(
    [string]$BuildDir = "build",
    [string]$Config = "RelWithDebInfo",
    [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount),
    [switch]$Clean,
    [switch]$UseGeode,
    [switch]$Ninja,
    [switch]$BuildOnly,
    [switch]$SkipConfigure,
    [switch]$UseCompilerCache = $true,
    [switch]$EnableHighPerformanceGpu
)

$ErrorActionPreference = "Stop"

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

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

if ($EnableHighPerformanceGpu) {
    Set-HighPerformanceGpuPreference -ToolNames @("geode", "cmake", "ninja")
    Write-Host "Applied Windows high-performance GPU preference for build tools (tool support dependent)."
}

if ($UseGeode) {
    $cmd = @("build", "--config", $Config)
    if ($Ninja) {
        $cmd += "--ninja"
    }
    if ($BuildOnly) {
        $cmd += "--build-only"
    }

    $launcher = Get-CompilerLauncher
    if ($launcher -and -not $BuildOnly) {
        $cmd += "--"
        $cmd += "-DCMAKE_C_COMPILER_LAUNCHER=$launcher"
        $cmd += "-DCMAKE_CXX_COMPILER_LAUNCHER=$launcher"
    }

    $previousParallelLevel = $env:CMAKE_BUILD_PARALLEL_LEVEL
    try {
        $env:CMAKE_BUILD_PARALLEL_LEVEL = [string]$Jobs
        Write-Host "Running: geode $($cmd -join ' ') (CMAKE_BUILD_PARALLEL_LEVEL=$env:CMAKE_BUILD_PARALLEL_LEVEL)"
        & geode @cmd
        if ($LASTEXITCODE -ne 0) {
            throw "Geode build failed."
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
else {
    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    $launcher = Get-CompilerLauncher
    $mustConfigure = -not $SkipConfigure -or -not (Test-Path $cachePath)

    if ($mustConfigure) {
        $configureCmd = @("-S", ".", "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=$Config")
        if ($launcher) {
            $configureCmd += "-DCMAKE_C_COMPILER_LAUNCHER=$launcher"
            $configureCmd += "-DCMAKE_CXX_COMPILER_LAUNCHER=$launcher"
        }

        Write-Host "Configuring CMake in '$BuildDir' with config '$Config'..."
        cmake @configureCmd
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed."
        }
    }
    else {
        Write-Host "Skipping CMake configure (cache found, -SkipConfigure set)."
    }

    Write-Host "Building with $Jobs parallel job(s)..."
    cmake --build $BuildDir --config $Config --parallel $Jobs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed."
    }
}

Write-Host "Build completed."
