Repository: https://github.com/Benno111/xdBot  
Original upstream: https://github.com/Zilko/xdBot

## Dependency installer

From the repo root, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install-dependencies.ps1 -PersistEnv
```

Optional Android binaries:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install-dependencies.ps1 -PersistEnv -InstallAndroid
```

Optional custom SDK path:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install-dependencies.ps1 -SdkPath "C:\Users\<you>\Documents\Geode"
```

## Build (multithreaded)

From the repo root, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

This uses all logical CPU cores by default. You can override the number of jobs:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Jobs 8
```

## Build Android

Build both Android targets:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-android.ps1
```

Build one target only:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-android.ps1 -Target Android32
powershell -ExecutionPolicy Bypass -File .\scripts\build-android.ps1 -Target Android64
```

Optional explicit NDK path:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-android.ps1 -Ndk "C:\Android\Sdk\ndk\27.1.12297006"
```
