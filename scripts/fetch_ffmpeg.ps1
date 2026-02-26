param(
    [string]$Target = "Windows"
)

$ErrorActionPreference = "Stop"

function Get-ArchiveKind {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$SourceUrl
    )

    $url = $SourceUrl.ToLowerInvariant()
    if ($url.EndsWith(".zip")) { return "zip" }
    if ($url.EndsWith(".tar.xz") -or $url.EndsWith(".tar.gz") -or $url.EndsWith(".tgz") -or $url.EndsWith(".tar")) {
        return "tar"
    }

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -ge 4) {
        # ZIP magic: PK..
        if ($bytes[0] -eq 0x50 -and $bytes[1] -eq 0x4B) { return "zip" }
        # GZIP magic: 1F 8B
        if ($bytes[0] -eq 0x1F -and $bytes[1] -eq 0x8B) { return "tar" }
    }

    return "unknown"
}

function Test-DownloadedArchive {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )

    if (-not (Test-Path $Path)) { return $false }
    $fileInfo = Get-Item -Path $Path
    if ($fileInfo.Length -lt 1024) { return $false }

    $prefix = Get-Content -Path $Path -TotalCount 1 -Encoding UTF8 -ErrorAction SilentlyContinue
    if ($prefix -and $prefix -match "<!DOCTYPE html|<html|<head|<body") {
        return $false
    }

    return $true
}

function Download-ArchiveWithFallbacks {
    param(
        [Parameter(Mandatory = $true)][string[]]$Urls,
        [Parameter(Mandatory = $true)][string]$OutFile
    )

    $attemptsPerUrl = 3
    foreach ($url in $Urls) {
        for ($i = 1; $i -le $attemptsPerUrl; $i++) {
            try {
                Write-Host "Downloading FFmpeg ($i/$attemptsPerUrl): $url"
                Invoke-WebRequest -Uri $url -OutFile $OutFile -MaximumRedirection 10 -Headers @{ "Accept" = "*/*" } -UserAgent "geobot-ci"

                if (Test-DownloadedArchive -Path $OutFile) {
                    return $url
                }

                throw "Downloaded file is not a valid archive payload."
            } catch {
                if (Test-Path $OutFile) {
                    Remove-Item -Path $OutFile -Force -ErrorAction SilentlyContinue
                }
                Write-Warning "Download attempt failed: $($_.Exception.Message)"
                Start-Sleep -Seconds ([Math]::Min(6, $i * 2))
            }
        }
    }

    throw "Failed to download FFmpeg from all candidate URLs."
}

function Download-And-Extract {
    param(
        [Parameter(Mandatory = $true)][string[]]$Urls,
        [Parameter(Mandatory = $true)][string]$ExtractDir
    )

    New-Item -ItemType Directory -Force -Path $ExtractDir | Out-Null

    $tempFile = Join-Path $env:RUNNER_TEMP ([System.IO.Path]::GetRandomFileName())
    $usedUrl = Download-ArchiveWithFallbacks -Urls $Urls -OutFile $tempFile
    $kind = Get-ArchiveKind -Path $tempFile -SourceUrl $usedUrl

    if ($kind -eq "zip") {
        Expand-Archive -Path $tempFile -DestinationPath $ExtractDir -Force
    } elseif ($kind -eq "tar") {
        tar -xf $tempFile -C $ExtractDir
    } else {
        throw "Unsupported FFmpeg archive format from: $usedUrl"
    }

    return $usedUrl
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

$windowsUrls = @(
    "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip",
    "https://github.com/BtbN/FFmpeg-Builds/releases/latest/download/ffmpeg-master-latest-win64-gpl.zip",
    "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip"
)

$targetNormalized = $Target.ToLowerInvariant()
$downloadUrls = @($windowsUrls)

if ($targetNormalized -eq "android32") {
    if ($env:FFMPEG_ANDROID32_URL) {
        $downloadUrls = @($env:FFMPEG_ANDROID32_URL) + $windowsUrls
    } else {
        Write-Warning "FFMPEG_ANDROID32_URL not set; falling back to Windows FFmpeg binary."
    }
} elseif ($targetNormalized -eq "android64") {
    if ($env:FFMPEG_ANDROID64_URL) {
        $downloadUrls = @($env:FFMPEG_ANDROID64_URL) + $windowsUrls
    } else {
        Write-Warning "FFMPEG_ANDROID64_URL not set; falling back to Windows FFmpeg binary."
    }
}

$extractDir = Join-Path $env:RUNNER_TEMP ("ffmpeg_extract_" + [Guid]::NewGuid().ToString("N"))
$usedDownloadUrl = Download-And-Extract -Urls $downloadUrls -ExtractDir $extractDir
$ffmpegBinary = Find-FFmpegBinary -Dir $extractDir

$bundleExePath = Join-Path $resourcesDir "ffmpeg.exe"
$bundleUnixPath = Join-Path $resourcesDir "ffmpeg"

Copy-Item -Path $ffmpegBinary -Destination $bundleExePath -Force
Copy-Item -Path $ffmpegBinary -Destination $bundleUnixPath -Force

if (-not $IsWindows) {
    chmod +x $bundleExePath
    chmod +x $bundleUnixPath
}

Write-Host "Bundled FFmpeg from: $usedDownloadUrl"
Write-Host "Bundled paths:"
Write-Host " - $bundleExePath"
Write-Host " - $bundleUnixPath"
