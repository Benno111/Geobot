param(
    [string]$BuildDir = "build",
    [string]$Config = "RelWithDebInfo",
    [string]$ModTarget = "geobot2",
    [string]$GeodeBindingsLib = "",
    [bool]$SuppressRegen = $true,
    [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount),
    [switch]$Clean,
    [switch]$UseGeode,
    [switch]$Ninja,
    [switch]$BuildOnly,
    [switch]$SkipConfigure,
    [switch]$UseCompilerCache = $true,
    [switch]$EnableHighPerformanceGpu,
    [switch]$UsePrebuilt,
    [switch]$UsePrecompiledGeodeBindings,
    [switch]$ForceConfigure,
    [switch]$QuietNinja,
    [switch]$FastRebuild,
    [switch]$BundleFFmpeg = $true,
    [string]$FFmpegSource = ""
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

function Resolve-FFmpegSourcePath {
    param(
        [string]$ExplicitSource
    )

    if (-not $IsWindows) {
        return $null
    }

    if (-not [string]::IsNullOrWhiteSpace($ExplicitSource)) {
        if (Test-Path $ExplicitSource -PathType Leaf) {
            $resolved = (Resolve-Path $ExplicitSource).Path
            if ([System.IO.Path]::GetFileName($resolved).ToLowerInvariant() -eq "ffmpeg.exe") {
                return $resolved
            }
        }
        throw "FFmpeg source path must point to ffmpeg.exe: '$ExplicitSource'"
    }

    $existingBundled = Join-Path "resources" "ffmpeg.exe"
    if (Test-Path $existingBundled -PathType Leaf) {
        return (Resolve-Path $existingBundled).Path
    }

    $ffmpegCmd = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($ffmpegCmd -and -not [string]::IsNullOrWhiteSpace($ffmpegCmd.Source) -and (Test-Path $ffmpegCmd.Source -PathType Leaf)) {
        if ([System.IO.Path]::GetFileName($ffmpegCmd.Source).ToLowerInvariant() -eq "ffmpeg.exe") {
            return (Resolve-Path $ffmpegCmd.Source).Path
        }
    }

    return $null
}

function Sync-BundledFFmpeg {
    param(
        [string]$ExplicitSource
    )

    if (-not $BundleFFmpeg -or -not $IsWindows) {
        return
    }

    $targetPath = Join-Path "resources" "ffmpeg.exe"
    $sourcePath = Resolve-FFmpegSourcePath -ExplicitSource $ExplicitSource
    if (-not $sourcePath) {
        Write-Host "FFmpeg bundling skipped (ffmpeg.exe not found)."
        return
    }

    $targetDir = Split-Path -Parent $targetPath
    if (-not (Test-Path $targetDir)) {
        New-Item -ItemType Directory -Path $targetDir | Out-Null
    }

    if ((Test-Path $targetPath -PathType Leaf)) {
        $src = Get-Item $sourcePath
        $dst = Get-Item $targetPath
        if ($src.Length -eq $dst.Length -and $src.LastWriteTimeUtc -eq $dst.LastWriteTimeUtc) {
            Write-Host "Bundled ffmpeg.exe is up to date."
            return
        }
    }

    Copy-Item -Path $sourcePath -Destination $targetPath -Force
    $srcAfter = Get-Item $sourcePath
    (Get-Item $targetPath).LastWriteTimeUtc = $srcAfter.LastWriteTimeUtc
    Write-Host "Bundled ffmpeg.exe -> $targetPath"
}

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

if ($FastRebuild) {
    $UsePrecompiledGeodeBindings = $true
    $UsePrebuilt = $true
    $QuietNinja = $true
    $SkipConfigure = $true
    $UseGeode = $true
    $BuildOnly = $true
}

if (-not $Clean -and -not $UseGeode -and -not $UsePrebuilt -and -not $UsePrecompiledGeodeBindings -and -not $ForceConfigure) {
    $cachePathAuto = Join-Path $BuildDir "CMakeCache.txt"
    $rootBindingsAuto = Join-Path "." "GeodeBindings-win.lib"
    if ((Test-Path $cachePathAuto) -and (Test-Path $rootBindingsAuto)) {
        Write-Host "Auto fast-path: found cached build + root GeodeBindings-win.lib, enabling precompiled bindings mode."
        $UsePrecompiledGeodeBindings = $true
    }
}

if ($UsePrebuilt) {
    $UseGeode = $true
    $BuildOnly = $true
    $SkipConfigure = $true
}

if ($UsePrecompiledGeodeBindings) {
    $UsePrebuilt = $true
    $UseGeode = $true
    $BuildOnly = $true
    $SkipConfigure = $true
}

if ($EnableHighPerformanceGpu) {
    Set-HighPerformanceGpuPreference -ToolNames @("geode", "cmake", "ninja")
    Write-Host "Applied Windows high-performance GPU preference for build tools (tool support dependent)."
}

Sync-BundledFFmpeg -ExplicitSource $FFmpegSource

$localBindingsRepo = Join-Path $BuildDir "_deps\bindings-src"
if ((-not $env:GEODE_BINDINGS_REPO_PATH) -and (Test-Path $localBindingsRepo)) {
    $env:GEODE_BINDINGS_REPO_PATH = (Resolve-Path $localBindingsRepo).Path
    Write-Host "Using local bindings repo path: $env:GEODE_BINDINGS_REPO_PATH"
}

if ($UseGeode) {
    if ($UsePrebuilt -and -not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
        throw "Prebuilt mode requires an existing configured Geode build folder (build/CMakeCache.txt). Run one full build first."
    }

    if ($UsePrecompiledGeodeBindings) {
        $bindingsCandidates = @()
        if (-not [string]::IsNullOrWhiteSpace($GeodeBindingsLib)) {
            $bindingsCandidates += $GeodeBindingsLib
        }
        $bindingsCandidates += @(
            (Join-Path "." "GeodeBindings-win.lib"),
            (Join-Path $BuildDir "bindings\GeodeBindings-win.lib"),
            (Join-Path $BuildDir "bindings\GeodeBindings.lib")
        )

        $bindingsLib = $bindingsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
        if (-not $bindingsLib) {
            throw "Precompiled GeodeBindings not found. Checked: $($bindingsCandidates -join ', '). Run one full build first."
        }
        Write-Host "Using precompiled GeodeBindings at '$bindingsLib'"
    }

    if ($UsePrebuilt) {
        $buildCmd = @("--build", $BuildDir, "--config", $Config, "--target", $ModTarget, "--parallel", $Jobs)
        if ($QuietNinja) {
            $buildCmd += "--"
            $buildCmd += "--quiet"
        }
        Write-Host "Running strict prebuilt target build: cmake $($buildCmd -join ' ')"
        cmake @buildCmd
        if ($LASTEXITCODE -ne 0) {
            throw "Prebuilt target build failed."
        }
        Write-Host "Build completed."
        exit 0
    }

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
    $mustConfigure = $ForceConfigure -or (-not (Test-Path $cachePath))

    if ($mustConfigure) {
        $configureCmd = @("-S", ".", "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=$Config")
        if ($SuppressRegen) {
            $configureCmd += "-DCMAKE_SUPPRESS_REGENERATION=ON"
        }
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
