param(
    [ValidateSet("Android32", "Android64", "Both")]
    [string]$Target = "Both",
    [string]$Config = "Release",
    [string]$Ndk = $env:ANDROID_NDK_ROOT,
    [switch]$SkipSdkSync
)

$ErrorActionPreference = "Stop"

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
        [string]$Ndk
    )

    $cmd = @("build", "-p", $Platform, "--config", $Config)
    if (-not [string]::IsNullOrWhiteSpace($Ndk)) {
        $cmd += @("--ndk", $Ndk)
    }

    Write-Host "Running: geode $($cmd -join ' ')"
    & geode @cmd
    if ($LASTEXITCODE -ne 0) {
        throw "Android build failed for $Platform."
    }
}

if (-not $SkipSdkSync) {
    $geodeVersion = Get-ModGeodeVersion
    Ensure-SdkVersion -RequiredVersion $geodeVersion
}
else {
    $geodeVersion = Get-ModGeodeVersion
}

if ($Target -eq "Android32" -or $Target -eq "Both") {
    Ensure-AndroidBinaries -Platform "android32" -GeodeVersion $geodeVersion
    Invoke-AndroidBuild -Platform "android32" -Config $Config -Ndk $Ndk
}

if ($Target -eq "Android64" -or $Target -eq "Both") {
    Ensure-AndroidBinaries -Platform "android64" -GeodeVersion $geodeVersion
    Invoke-AndroidBuild -Platform "android64" -Config $Config -Ndk $Ndk
}

Write-Host "Android build completed."
