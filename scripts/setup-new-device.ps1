param(
    [string]$SdkPath = (Join-Path $HOME "Documents\Geode"),
    [switch]$InstallAndroid,
    [switch]$PersistEnv = $true
)

$ErrorActionPreference = "Stop"

function Test-CommandExists {
    param([string]$Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Install-WingetPackageIfMissing {
    param(
        [string]$CommandName,
        [string]$PackageId
    )

    if (Test-CommandExists $CommandName) {
        Write-Host "$CommandName already installed."
        return
    }

    if (-not (Test-CommandExists "winget")) {
        throw "winget is required to install missing dependencies ($PackageId)."
    }

    Write-Host "Installing $PackageId..."
    & winget install --id $PackageId --exact --silent --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install $PackageId."
    }
}

function Ensure-GeodeCli {
    if (Test-CommandExists "geode") {
        return (Get-Command geode).Source
    }

    if (-not (Test-CommandExists "scoop")) {
        Write-Host "Installing Scoop..."
        Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser -Force
        Invoke-RestMethod -Uri https://get.scoop.sh | Invoke-Expression
    }

    Write-Host "Installing geode CLI via Scoop..."
    & scoop install geode
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install geode CLI with Scoop. Install it manually and rerun this script."
    }

    return (Get-Command geode).Source
}

function Get-ModGeodeVersion {
    $projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
    $modJsonPath = Join-Path $projectRoot "mod.json"
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

Install-WingetPackageIfMissing -CommandName "git" -PackageId "Git.Git"
Install-WingetPackageIfMissing -CommandName "cmake" -PackageId "Kitware.CMake"
Install-WingetPackageIfMissing -CommandName "ninja" -PackageId "Ninja-build.Ninja"
Install-WingetPackageIfMissing -CommandName "python" -PackageId "Python.Python.3.12"

$geodeExe = Ensure-GeodeCli
$geodeVersion = Get-ModGeodeVersion

if (-not (Test-Path $SdkPath)) {
    New-Item -ItemType Directory -Path $SdkPath | Out-Null
}

Write-Host "Using Geode CLI: $geodeExe"
Write-Host "Using Geode SDK path: $SdkPath"
Write-Host "Using mod Geode version: $geodeVersion"

if ($PersistEnv) {
    setx GEODE_SDK $SdkPath | Out-Null
    $env:GEODE_SDK = $SdkPath
    Write-Host "Persisted GEODE_SDK."
}
else {
    $env:GEODE_SDK = $SdkPath
}

& $geodeExe sdk set-path $SdkPath
if ($LASTEXITCODE -ne 0) {
    throw "Failed to set Geode SDK path."
}

& $geodeExe sdk install-binaries -p win -v $geodeVersion
if ($LASTEXITCODE -ne 0) {
    throw "Failed to install Windows Geode binaries."
}

if ($InstallAndroid) {
    & $geodeExe sdk install-binaries -p android -v $geodeVersion
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install Android Geode binaries."
    }
}

Write-Host "Setup complete."
