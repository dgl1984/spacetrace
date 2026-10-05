$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root 'build-visual-snapshots'
$Output = Join-Path $Build 'visual-snapshots'
$Log = Join-Path $Root 'BUILD_VISUAL_SNAPSHOTS_LOG.txt'
$Juce = if ($env:JUCE_DIR) { $env:JUCE_DIR } else { Join-Path $Root 'deps\JUCE' }

Start-Transcript -Path $Log -Force | Out-Null
try {
    Write-Host '=== SpaceTrace 1.0 visual snapshot harness ==='
    Write-Host "Source: $Root"

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'CMake is required and was not found on PATH.' }
    if (-not (Test-Path (Join-Path $Juce 'CMakeLists.txt'))) {
        if ($env:JUCE_DIR) { throw "JUCE_DIR does not contain CMakeLists.txt: $Juce" }
        Write-Host 'Pinned JUCE 8.0.12 is missing; fetching the verified dependency.'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'FETCH_JUCE_WINDOWS.ps1') | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "JUCE fetch failed with exit code $LASTEXITCODE" }
    }

    Write-Host '=== Configure developer snapshot target ==='
    & cmake -S $Root -B $Build -G 'Visual Studio 17 2022' -A x64 `
        -DSPACETRACE_BUILD_PLUGIN=ON `
        -DSPACETRACE_BUILD_TESTS=OFF `
        -DSPACETRACE_BUILD_TOOLS=OFF `
        -DSPACETRACE_BUILD_CLAP=OFF `
        -DSPACETRACE_BUILD_VISUAL_SNAPSHOTS=ON `
        "-DJUCE_DIR=$Juce" | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

    Write-Host '=== Build visual snapshot executable ==='
    & cmake --build $Build --config Release --target spacetrace_visual_snapshots | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Snapshot build failed with exit code $LASTEXITCODE" }

    if (Test-Path -LiteralPath $Output) { Remove-Item -LiteralPath $Output -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $Output | Out-Null

    $Exe = Join-Path $Build 'Release\spacetrace_visual_snapshots.exe'
    if (-not (Test-Path -LiteralPath $Exe)) {
        $Found = Get-ChildItem -Path $Build -Filter 'spacetrace_visual_snapshots.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($Found) { $Exe = $Found.FullName } else { throw 'Snapshot executable could not be located.' }
    }

    Write-Host '=== Render real SpaceTrace editor states ==='
    $PreviousHeads = $env:SPACETRACE_HEADS_DIR
    try {
        $env:SPACETRACE_HEADS_DIR = Join-Path $Root 'Heads'
        & $Exe $Output | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "Snapshot renderer failed with exit code $LASTEXITCODE" }
    }
    finally {
        $env:SPACETRACE_HEADS_DIR = $PreviousHeads
    }

    Write-Host '=== PASS ==='
    Write-Host "Snapshots: $Output"
    Write-Host "Manifest: $(Join-Path $Output 'manifest.txt')"
    Write-Host "Report: $(Join-Path $Output 'VISUAL_SNAPSHOT_REPORT.md')"
    Write-Host "Build log: $Log"
    Write-Host 'Scale-factor snapshots are component-render checks, not simulated Windows DPI.'
}
catch {
    Write-Host '=== FAIL ==='
    Write-Host ("ERROR: {0}" -f $_.Exception.Message)
    throw
}
finally {
    Stop-Transcript | Out-Null
}
