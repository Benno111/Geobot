param(
    [string]$Target = "Windows"
)

$ErrorActionPreference = "Stop"

function Download-And-Extract {
    param(
        [Parameter(Mandatory = $true)][string]$Url,
        [Parameter(Mandatory = $true)][string]$ExtractDir
    )

    New-Item -ItemType Directory -Force -Path $ExtractDir | Out-Null

    $tempFile = Join-Path $env:RUNNER_TEMP ([System.IO.Path]::GetRandomFileName())
    Invoke-WebRequest -Uri $Url -OutFile $tempFile

    $lowerUrl = $Url.ToLowerInvariant()
    if ($lowerUrl.EndsWith(".zip")) {
        Expand-Archive -Path $tempFile -DestinationPath $ExtractDir -Force
    } elseif ($lowerUrl.EndsWith(".tar.xz") -or $lowerUrl.EndsWith(".tar.gz") -or $lowerUrl.EndsWith(".tgz") -or $lowerUrl.EndsWith(".tar")) {
        tar -xf $tempFile -C $ExtractDir
    } else {
        throw "Unsupported FFmpeg archive format: $Url"
    }
}

function Find-FFmpegBinary {
    param(
        [Parameter(Mandatory = $true)][string]$Dir
    )

    $candidates = Get-ChildItem -Path $Dir -Recurse -File | Where-Object {
        $_.Name -ieq "ffmpeg.exe" -or $_.Name -ieq "ffmpeg"
    }

    if (-not $candidates -or $candidates.Count -eq 0) {
        throw "FFmpeg binary not found in extracted archive: $Dir"
    }

    $exe = $candidates | Where-Object { $_.Name -ieq "ffmpeg.exe" } | Select-Object -First 1
    if ($exe) { return $exe.FullName }
    return ($candidates | Select-Object -First 1).FullName
}

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$resourcesDir = Join-Path $root "resources"
New-Item -ItemType Directory -Force -Path $resourcesDir | Out-Null

$windowsUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip"

$targetNormalized = $Target.ToLowerInvariant()
$downloadUrl = $windowsUrl

if ($targetNormalized -eq "android32") {
    if ($env:FFMPEG_ANDROID32_URL) {
        $downloadUrl = $env:FFMPEG_ANDROID32_URL
    } else {
        Write-Warning "FFMPEG_ANDROID32_URL not set; falling back to Windows FFmpeg binary."
    }
} elseif ($targetNormalized -eq "android64") {
    if ($env:FFMPEG_ANDROID64_URL) {
        $downloadUrl = $env:FFMPEG_ANDROID64_URL
    } else {
        Write-Warning "FFMPEG_ANDROID64_URL not set; falling back to Windows FFmpeg binary."
    }
}

$extractDir = Join-Path $env:RUNNER_TEMP ("ffmpeg_extract_" + [Guid]::NewGuid().ToString("N"))
Download-And-Extract -Url $downloadUrl -ExtractDir $extractDir
$ffmpegBinary = Find-FFmpegBinary -Dir $extractDir

$bundleExePath = Join-Path $resourcesDir "ffmpeg.exe"
$bundleUnixPath = Join-Path $resourcesDir "ffmpeg"

Copy-Item -Path $ffmpegBinary -Destination $bundleExePath -Force
Copy-Item -Path $ffmpegBinary -Destination $bundleUnixPath -Force

if (-not $IsWindows) {
    chmod +x $bundleExePath
    chmod +x $bundleUnixPath
}

Write-Host "Bundled FFmpeg from: $downloadUrl"
Write-Host "Bundled paths:"
Write-Host " - $bundleExePath"
Write-Host " - $bundleUnixPath"
