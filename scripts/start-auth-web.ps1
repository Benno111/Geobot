param(
    [int]$Port = 8080,
    [string]$ConfigPath = (Join-Path $HOME "Desktop\geobot-server-config.json"),
    [string]$RootPath = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

function Write-JsonResponse {
    param(
        [System.Net.HttpListenerResponse]$Response,
        [int]$StatusCode,
        [hashtable]$Body
    )

    $json = ($Body | ConvertTo-Json -Depth 8)
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    $Response.StatusCode = $StatusCode
    $Response.ContentType = "application/json; charset=utf-8"
    $Response.ContentLength64 = $bytes.Length
    $Response.OutputStream.Write($bytes, 0, $bytes.Length)
    $Response.OutputStream.Close()
}

function Write-TextResponse {
    param(
        [System.Net.HttpListenerResponse]$Response,
        [int]$StatusCode,
        [string]$Body,
        [string]$ContentType = "text/plain; charset=utf-8"
    )

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Body)
    $Response.StatusCode = $StatusCode
    $Response.ContentType = $ContentType
    $Response.ContentLength64 = $bytes.Length
    $Response.OutputStream.Write($bytes, 0, $bytes.Length)
    $Response.OutputStream.Close()
}

function Get-Config {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        $template = @{
            host = "127.0.0.1"
            username = "admin"
            password = "change-me-now"
            api_key = "geobot-local-key"
        }
        $template | ConvertTo-Json -Depth 3 | Set-Content -Path $Path -Encoding UTF8
        throw "Config created at '$Path'. Set strong credentials and rerun."
    }

    $cfg = Get-Content -Raw -Path $Path | ConvertFrom-Json
    if (-not $cfg.username -or -not $cfg.password -or -not $cfg.api_key) {
        throw "Invalid config. Expected username/password/api_key."
    }
    return $cfg
}

function Test-Auth {
    param(
        [System.Net.HttpListenerRequest]$Request,
        $Config
    )

    $apiKey = $Request.Headers["X-GeoBot-Auth"]
    if ($apiKey -and $apiKey -eq [string]$Config.api_key) {
        return $true
    }

    $auth = $Request.Headers["Authorization"]
    if (-not $auth -or -not $auth.StartsWith("Basic ")) {
        return $false
    }

    try {
        $raw = [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($auth.Substring(6)))
        $parts = $raw.Split(":", 2)
        if ($parts.Length -ne 2) {
            return $false
        }
        return ($parts[0] -eq [string]$Config.username -and $parts[1] -eq [string]$Config.password)
    }
    catch {
        return $false
    }
}

function Resolve-SafePath {
    param(
        [string]$BasePath,
        [string]$RelativePath
    )

    $full = [System.IO.Path]::GetFullPath((Join-Path $BasePath $RelativePath))
    $base = [System.IO.Path]::GetFullPath($BasePath)
    if (-not $full.StartsWith($base, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes RootPath."
    }
    return $full
}

function Read-RequestBody {
    param([System.Net.HttpListenerRequest]$Request)
    $reader = New-Object System.IO.StreamReader($Request.InputStream, $Request.ContentEncoding)
    try {
        return $reader.ReadToEnd()
    }
    finally {
        $reader.Close()
    }
}

function Parse-RequestJson {
    param([System.Net.HttpListenerRequest]$Request)
    $body = Read-RequestBody -Request $Request
    if ([string]::IsNullOrWhiteSpace($body)) {
        return @{}
    }
    return $body | ConvertFrom-Json
}

function Invoke-ProcessCapture {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$WorkingDirectory
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    foreach ($arg in $Arguments) {
        [void]$psi.ArgumentList.Add($arg)
    }
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    [void]$proc.Start()

    $stdout = $proc.StandardOutput.ReadToEnd()
    $stderr = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()

    return @{
        exit_code = $proc.ExitCode
        stdout = $stdout
        stderr = $stderr
        ok = ($proc.ExitCode -eq 0)
        cmd = "$FilePath $($Arguments -join ' ')"
    }
}

function Get-GitBranches {
    param([string]$RepoPath)
    $result = Invoke-ProcessCapture -FilePath "git" -Arguments @("-C", $RepoPath, "for-each-ref", "--format=%(refname:short)", "refs/heads") -WorkingDirectory $RepoPath
    if (-not $result.ok) {
        throw "Failed to list local branches: $($result.stderr)"
    }
    return ($result.stdout -split "`r?`n" | Where-Object { $_ -and $_.Trim() -ne "" })
}

function Get-GitCurrentBranch {
    param([string]$RepoPath)
    $result = Invoke-ProcessCapture -FilePath "git" -Arguments @("-C", $RepoPath, "rev-parse", "--abbrev-ref", "HEAD") -WorkingDirectory $RepoPath
    if (-not $result.ok) {
        throw "Failed to get current branch: $($result.stderr)"
    }
    return $result.stdout.Trim()
}

function Get-GitRemoteBranches {
    param([string]$RepoPath)
    $result = Invoke-ProcessCapture -FilePath "git" -Arguments @("-C", $RepoPath, "for-each-ref", "--format=%(refname:short)", "refs/remotes/origin") -WorkingDirectory $RepoPath
    if (-not $result.ok) {
        throw "Failed to list remote branches: $($result.stderr)"
    }
    return ($result.stdout -split "`r?`n" | Where-Object { $_ -and $_.Trim() -ne "" })
}

function Switch-GitBranch {
    param(
        [string]$RepoPath,
        [string]$Branch
    )

    if ($Branch -notmatch '^[A-Za-z0-9._/\-]+$') {
        throw "Invalid branch name."
    }

    $local = Get-GitBranches -RepoPath $RepoPath
    if ($local -contains $Branch) {
        return Invoke-ProcessCapture -FilePath "git" -Arguments @("-C", $RepoPath, "switch", $Branch) -WorkingDirectory $RepoPath
    }

    $remote = Get-GitRemoteBranches -RepoPath $RepoPath
    $remoteRef = "origin/$Branch"
    if ($remote -contains $remoteRef) {
        return Invoke-ProcessCapture -FilePath "git" -Arguments @("-C", $RepoPath, "switch", "-c", $Branch, "--track", $remoteRef) -WorkingDirectory $RepoPath
    }

    throw "Branch '$Branch' not found locally or on origin."
}

function Invoke-BuildFromOptions {
    param(
        [string]$RepoPath,
        $Body
    )

    $platform = "win"
    if ($Body.platform) {
        $platform = [string]$Body.platform
    }
    $jobs = if ($Body.jobs) { [int]$Body.jobs } else { [Environment]::ProcessorCount }
    if ($jobs -lt 1) { $jobs = 1 }

    if ($platform -eq "win") {
        $args = @("-ExecutionPolicy", "Bypass", "-File", (Join-Path $RepoPath "scripts\build.ps1"), "-Jobs", [string]$jobs)

        if ($Body.config) { $args += @("-Config", [string]$Body.config) }
        if ($Body.ninja) { $args += "-Ninja" }
        if ($Body.fast_rebuild) { $args += "-FastRebuild" }
        if ($Body.force_configure) { $args += "-ForceConfigure" }
        if ($Body.clean) { $args += "-Clean" }

        return Invoke-ProcessCapture -FilePath "powershell" -Arguments $args -WorkingDirectory $RepoPath
    }

    $androidTarget = switch ($platform) {
        "android32" { "Android32" }
        "android64" { "Android64" }
        "android-both" { "Both" }
        default { throw "Unsupported platform '$platform'." }
    }

    $args = @(
        "-ExecutionPolicy", "Bypass",
        "-File", (Join-Path $RepoPath "scripts\build-android.ps1"),
        "-Target", $androidTarget,
        "-Jobs", [string]$jobs
    )

    if ($Body.config) { $args += @("-Config", [string]$Body.config) }
    if ($Body.ninja) { $args += "-Ninja" }
    if ($Body.fast) { $args += "-Fast" }
    if ($Body.build_only) { $args += "-BuildOnly" }
    if ($Body.ndk) { $args += @("-Ndk", [string]$Body.ndk) }

    return Invoke-ProcessCapture -FilePath "powershell" -Arguments $args -WorkingDirectory $RepoPath
}

function Get-GuiHtml {
@'
<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>GeoBot Build Console</title>
  <style>
    body { font-family: Segoe UI, Arial, sans-serif; margin: 18px; background:#0f1218; color:#e8ecf2; }
    .card { background:#161b24; border:1px solid #2a3242; border-radius:8px; padding:14px; margin-bottom:14px; }
    label { display:block; margin-top:8px; font-size:12px; color:#9fb0c6; }
    input, select { width:100%; padding:8px; margin-top:4px; background:#0f1218; color:#e8ecf2; border:1px solid #2a3242; border-radius:6px; }
    button { margin-top:10px; padding:8px 12px; background:#2a87ff; border:none; border-radius:6px; color:white; cursor:pointer; }
    button:disabled { opacity:.6; cursor:not-allowed; }
    pre { white-space: pre-wrap; background:#0a0d12; padding:10px; border-radius:6px; border:1px solid #2a3242; max-height:420px; overflow:auto; }
    .row { display:grid; grid-template-columns:1fr 1fr; gap:12px; }
    .muted { color:#9fb0c6; font-size:12px; }
  </style>
</head>
<body>
  <h2>GeoBot Build Console</h2>
  <div class="muted" id="statusLine">Loading...</div>

  <div class="card">
    <h3>Branch</h3>
    <label>Current branch</label>
    <input id="currentBranch" disabled />
    <label>Switch to branch</label>
    <select id="branchSelect"></select>
    <button id="switchBtn">Switch Branch</button>
  </div>

  <div class="card">
    <h3>Build</h3>
    <div class="row">
      <div>
        <label>Platform</label>
        <select id="platform">
          <option value="win">Windows</option>
          <option value="android32">Android32</option>
          <option value="android64">Android64</option>
          <option value="android-both">Android Both</option>
        </select>
      </div>
      <div>
        <label>Jobs</label>
        <input id="jobs" type="number" min="1" value="8" />
      </div>
    </div>
    <div class="row">
      <div>
        <label>Config</label>
        <select id="config">
          <option value="RelWithDebInfo">RelWithDebInfo</option>
          <option value="Release">Release</option>
          <option value="Debug">Debug</option>
        </select>
      </div>
      <div>
        <label>Ninja</label>
        <select id="ninja">
          <option value="true">true</option>
          <option value="false">false</option>
        </select>
      </div>
    </div>
    <label>Mode</label>
    <select id="mode">
      <option value="fast">Fast</option>
      <option value="normal">Normal</option>
      <option value="buildonly">BuildOnly</option>
    </select>
    <button id="buildBtn">Run Build</button>
  </div>

  <div class="card">
    <h3>Output</h3>
    <pre id="output"></pre>
  </div>

  <script>
    const out = document.getElementById('output');
    const statusLine = document.getElementById('statusLine');
    const branchSelect = document.getElementById('branchSelect');
    const currentBranch = document.getElementById('currentBranch');
    const switchBtn = document.getElementById('switchBtn');
    const buildBtn = document.getElementById('buildBtn');

    function setOutput(text) { out.textContent = text || ''; }

    async function api(path, opts = {}) {
      const res = await fetch(path, { ...opts, headers: { 'Content-Type': 'application/json', ...(opts.headers || {}) } });
      const txt = await res.text();
      let data;
      try { data = JSON.parse(txt); } catch { data = { raw: txt }; }
      if (!res.ok) throw new Error(data.error || txt || ('HTTP ' + res.status));
      return data;
    }

    async function refreshStatus() {
      try {
        const st = await api('/api/status');
        statusLine.textContent = 'Repo: ' + st.repo + ' | Current: ' + st.current_branch;
        currentBranch.value = st.current_branch;
      } catch (e) {
        statusLine.textContent = 'Status error: ' + e.message;
      }
    }

    async function refreshBranches() {
      const data = await api('/api/branches');
      branchSelect.innerHTML = '';
      for (const b of data.local_branches) {
        const o = document.createElement('option');
        o.value = b;
        o.textContent = b;
        if (b === data.current_branch) o.selected = true;
        branchSelect.appendChild(o);
      }
    }

    switchBtn.addEventListener('click', async () => {
      switchBtn.disabled = true;
      try {
        const branch = branchSelect.value;
        setOutput('Switching branch to ' + branch + '...');
        const data = await api('/api/checkout', { method: 'POST', body: JSON.stringify({ branch }) });
        setOutput((data.stdout || '') + '\n' + (data.stderr || ''));
        await refreshStatus();
        await refreshBranches();
      } catch (e) {
        setOutput('ERROR: ' + e.message);
      } finally {
        switchBtn.disabled = false;
      }
    });

    buildBtn.addEventListener('click', async () => {
      buildBtn.disabled = true;
      try {
        const platform = document.getElementById('platform').value;
        const jobs = parseInt(document.getElementById('jobs').value || '8', 10);
        const config = document.getElementById('config').value;
        const ninja = document.getElementById('ninja').value === 'true';
        const mode = document.getElementById('mode').value;
        const body = { platform, jobs, config, ninja };
        if (platform === 'win') {
          body.fast_rebuild = mode === 'fast';
          body.build_only = mode === 'buildonly';
        } else {
          body.fast = mode === 'fast';
          body.build_only = mode === 'buildonly';
        }

        setOutput('Starting build...');
        const data = await api('/api/build', { method: 'POST', body: JSON.stringify(body) });
        setOutput('exit_code=' + data.exit_code + '\n\n' + (data.stdout || '') + '\n' + (data.stderr || ''));
      } catch (e) {
        setOutput('ERROR: ' + e.message);
      } finally {
        buildBtn.disabled = false;
      }
    });

    (async () => {
      await refreshStatus();
      await refreshBranches();
    })();
  </script>
</body>
</html>
'@
}

$config = Get-Config -Path $ConfigPath
$host = if ($config.host) { [string]$config.host } else { "127.0.0.1" }
$prefix = "http://$host`:$Port/"

$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add($prefix)
$listener.Start()

Write-Host "GeoBot auth server running at $prefix"
Write-Host "Root path: $RootPath"
Write-Host "Config path: $ConfigPath"

try {
    while ($listener.IsListening) {
        $ctx = $listener.GetContext()
        $req = $ctx.Request
        $res = $ctx.Response

        if (-not (Test-Auth -Request $req -Config $config)) {
            $res.StatusCode = 401
            $res.Headers["WWW-Authenticate"] = 'Basic realm="geobot-local"'
            Write-JsonResponse -Response $res -StatusCode 401 -Body @{ error = "Unauthorized" }
            continue
        }

        $path = $req.Url.AbsolutePath.Trim("/")

        if ($req.HttpMethod -eq "GET" -and ($path -eq "" -or $path -eq "ui")) {
            Write-TextResponse -Response $res -StatusCode 200 -Body (Get-GuiHtml) -ContentType "text/html; charset=utf-8"
            continue
        }

        if ($req.HttpMethod -eq "GET" -and $path -eq "health") {
            Write-JsonResponse -Response $res -StatusCode 200 -Body @{ ok = $true; service = "geobot-auth-web" }
            continue
        }

        if ($req.HttpMethod -eq "GET" -and $path -eq "api/status") {
            try {
                $current = Get-GitCurrentBranch -RepoPath $RootPath
                Write-JsonResponse -Response $res -StatusCode 200 -Body @{ ok = $true; repo = $RootPath; current_branch = $current }
            } catch {
                Write-JsonResponse -Response $res -StatusCode 500 -Body @{ error = $_.Exception.Message }
            }
            continue
        }

        if ($req.HttpMethod -eq "GET" -and $path -eq "api/branches") {
            try {
                $local = Get-GitBranches -RepoPath $RootPath
                $current = Get-GitCurrentBranch -RepoPath $RootPath
                Write-JsonResponse -Response $res -StatusCode 200 -Body @{ ok = $true; current_branch = $current; local_branches = $local }
            } catch {
                Write-JsonResponse -Response $res -StatusCode 500 -Body @{ error = $_.Exception.Message }
            }
            continue
        }

        if ($req.HttpMethod -eq "POST" -and $path -eq "api/checkout") {
            try {
                $body = Parse-RequestJson -Request $req
                $branch = [string]$body.branch
                if ([string]::IsNullOrWhiteSpace($branch)) {
                    Write-JsonResponse -Response $res -StatusCode 400 -Body @{ error = "Missing branch." }
                    continue
                }
                $result = Switch-GitBranch -RepoPath $RootPath -Branch $branch
                if (-not $result.ok) {
                    Write-JsonResponse -Response $res -StatusCode 500 -Body $result
                } else {
                    Write-JsonResponse -Response $res -StatusCode 200 -Body $result
                }
            } catch {
                Write-JsonResponse -Response $res -StatusCode 400 -Body @{ error = $_.Exception.Message }
            }
            continue
        }

        if ($req.HttpMethod -eq "POST" -and $path -eq "api/build") {
            try {
                $body = Parse-RequestJson -Request $req
                $result = Invoke-BuildFromOptions -RepoPath $RootPath -Body $body
                if (-not $result.ok) {
                    Write-JsonResponse -Response $res -StatusCode 500 -Body $result
                } else {
                    Write-JsonResponse -Response $res -StatusCode 200 -Body $result
                }
            } catch {
                Write-JsonResponse -Response $res -StatusCode 400 -Body @{ error = $_.Exception.Message }
            }
            continue
        }

        if ($req.HttpMethod -eq "GET" -and $path -eq "read") {
            $relative = $req.QueryString["path"]
            if (-not $relative) {
                Write-JsonResponse -Response $res -StatusCode 400 -Body @{ error = "Missing query parameter: path" }
                continue
            }
            try {
                $file = Resolve-SafePath -BasePath $RootPath -RelativePath $relative
                if (-not (Test-Path $file)) {
                    Write-JsonResponse -Response $res -StatusCode 404 -Body @{ error = "File not found"; path = $relative }
                } else {
                    Write-JsonResponse -Response $res -StatusCode 200 -Body @{ path = $relative; content = (Get-Content -Raw -Path $file) }
                }
            } catch {
                Write-JsonResponse -Response $res -StatusCode 400 -Body @{ error = $_.Exception.Message }
            }
            continue
        }

        if ($req.HttpMethod -eq "POST" -and $path -eq "write") {
            $relative = $req.QueryString["path"]
            if (-not $relative) {
                Write-JsonResponse -Response $res -StatusCode 400 -Body @{ error = "Missing query parameter: path" }
                continue
            }

            $body = Read-RequestBody -Request $req

            try {
                $file = Resolve-SafePath -BasePath $RootPath -RelativePath $relative
                $dir = Split-Path -Parent $file
                if ($dir -and -not (Test-Path $dir)) {
                    New-Item -ItemType Directory -Path $dir -Force | Out-Null
                }
                Set-Content -Path $file -Value $body -Encoding UTF8
                Write-JsonResponse -Response $res -StatusCode 200 -Body @{ ok = $true; path = $relative }
            } catch {
                Write-JsonResponse -Response $res -StatusCode 400 -Body @{ error = $_.Exception.Message }
            }
            continue
        }

        Write-JsonResponse -Response $res -StatusCode 404 -Body @{ error = "Not found"; route = $path }
    }
}
finally {
    $listener.Stop()
    $listener.Close()
}
