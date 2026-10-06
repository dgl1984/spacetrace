$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root 'build-windows'
$Log = Join-Path $Root 'BUILD_WINDOWS_LOG.txt'
$Juce = if ($env:JUCE_DIR) { $env:JUCE_DIR } else { Join-Path $Root 'deps\JUCE' }
$ClapExt = if ($env:CLAP_JUCE_EXTENSIONS_DIR) { $env:CLAP_JUCE_EXTENSIONS_DIR } else { Join-Path $Root 'deps\clap-juce-extensions' }

Start-Transcript -Path $Log -Force | Out-Null
try {
    Write-Host '=== SpaceTrace 1.0.1 - VST3 + CLAP Validation ==='
    Write-Host "Source: $Root"

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'CMake is required and was not found on PATH.' }
    if (-not (Get-Command python -ErrorAction SilentlyContinue)) { throw 'Python is required and was not found on PATH.' }

    if (-not (Test-Path (Join-Path $Juce 'CMakeLists.txt'))) {
        if ($env:JUCE_DIR) {
            throw "JUCE_DIR does not contain CMakeLists.txt: $Juce"
        }
        Write-Host 'Pinned JUCE 8.0.12 is missing; fetching the verified dependency.'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'FETCH_JUCE_WINDOWS.ps1') | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "JUCE fetch failed with exit code $LASTEXITCODE" }
    }

    if ($env:CLAP_JUCE_EXTENSIONS_DIR) {
        if (-not (Test-Path (Join-Path $ClapExt 'CMakeLists.txt'))) {
            throw "CLAP_JUCE_EXTENSIONS_DIR does not contain CMakeLists.txt: $ClapExt"
        }
    } else {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'FETCH_CLAP_WINDOWS.ps1') | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "CLAP dependency verification failed with exit code $LASTEXITCODE" }
    }

    $Ku100Package = Join-Path $Root 'resources\datasets\sadie2_d1_ku100_44100.sthrtf'
    $Ku100Portable = Join-Path $Root 'Heads\KU100_SADIE_D1\head.sthrtf'
    $Ku100Manifest = Join-Path $Root 'Heads\KU100_SADIE_D1\manifest.json'
    $Ku100NeedsPreparation = $true
    if (Test-Path -LiteralPath $Ku100Package) {
        Write-Host '=== Checking existing SADIE II D1 / KU100 native package ==='
        & python (Join-Path $Root 'scripts\check_native_package.py') $Ku100Package `
            --name 'SADIE II D1 / Neumann KU100' `
            --measurement-count 8802 `
            --ir-length 256 `
            --require-compensation `
            --compensation-version 'tonetrace-raw-v1' | Out-Host
        if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $Ku100Portable) -and (Test-Path -LiteralPath $Ku100Manifest)) {
            $Ku100NeedsPreparation = $false
        } else {
            Write-Host 'Existing KU100 package is stale or uncorrected; rebuilding it from the official source/cache.'
        }
    }
    if ($Ku100NeedsPreparation) {
        Write-Host '=== Preparing official SADIE II D1 / KU100 dataset + measured correction ==='
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'scripts\Prepare-SADIE-D1.ps1') -Python 'python' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "SADIE II D1 preparation failed with exit code $LASTEXITCODE" }
    }

    Write-Host '=== Source contract ==='
    & python (Join-Path $Root 'tests\source_contract.py') | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Source contract failed with exit code $LASTEXITCODE" }

    Write-Host '=== External head package validation ==='
    & python (Join-Path $Root 'tests\validate_heads.py') --require-all | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "External head package validation failed with exit code $LASTEXITCODE" }

    # Keep the CMake cache and compiled dependencies for incremental builds when present.
    Write-Host "Build directory: $Build (incremental if already present)"

    Write-Host '=== Configure Visual Studio 2022 x64 ==='
    & cmake -S $Root -B $Build -G 'Visual Studio 17 2022' -A x64 `
        -DSPACETRACE_BUILD_PLUGIN=ON `
        -DSPACETRACE_BUILD_TESTS=ON `
        -DSPACETRACE_BUILD_TOOLS=OFF `
        -DSPACETRACE_BUILD_CLAP=ON `
        "-DJUCE_DIR=$Juce" `
        "-DCLAP_JUCE_EXTENSIONS_DIR=$ClapExt" | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

    Write-Host '=== Build tests and processor integration ==='
    & cmake --build $Build --config Release --target `
        spacetrace_core_tests spacetrace_dataset_tests spacetrace_plugin_tests spacetrace_clap_tests | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Test build failed with exit code $LASTEXITCODE" }

    Write-Host '=== Run tests ==='
    & ctest --test-dir $Build -C Release --output-on-failure | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "CTest failed with exit code $LASTEXITCODE" }

    Write-Host '=== Build VST3 and CLAP ==='
    & cmake --build $Build --config Release --target SpaceTrace_VST3 SpaceTrace_CLAP | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Plug-in build failed with exit code $LASTEXITCODE" }

    $Vst3 = Join-Path $Build 'SpaceTrace_artefacts\Release\VST3\SpaceTrace.vst3'
    if (-not (Test-Path $Vst3)) {
        $Found = Get-ChildItem -Path $Build -Filter 'SpaceTrace.vst3' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($Found) { $Vst3 = $Found.FullName } else { throw 'Build passed but SpaceTrace.vst3 could not be located.' }
    }

    $Clap = Join-Path $Build 'SpaceTrace_artefacts\Release\CLAP\SpaceTrace.clap'
    if (-not (Test-Path $Clap)) {
        $FoundClap = Get-ChildItem -Path $Build -Filter 'SpaceTrace.clap' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($FoundClap) { $Clap = $FoundClap.FullName } else { throw 'Build passed but SpaceTrace.clap could not be located.' }
    }

    Write-Host '=== Stage portable checkpoint ==='
    $PortableRoot = Join-Path $Build 'portable\SpaceTrace'
    $PortableFullPath = [System.IO.Path]::GetFullPath($PortableRoot)
    $BuildFullPath = [System.IO.Path]::GetFullPath($Build).TrimEnd('\') + '\'
    if (-not $PortableFullPath.StartsWith($BuildFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Portable staging path escapes the build directory: $PortableFullPath"
    }
    foreach ($Directory in @($Build, (Split-Path -Parent $PortableRoot), $PortableRoot)) {
        if ((Test-Path -LiteralPath $Directory) -and
            ((Get-Item -LiteralPath $Directory -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to clear portable output through a linked directory: $Directory"
        }
    }
    # Keep public staging whitelisted and testable. Do not copy whole Docs/, source,
    # third_party/, audits, handoffs, or build notes into the portable release.
    $StageScript = Join-Path $Root 'scripts\stage_release.py'
    & python $StageScript --source-root $Root --vst3 $Vst3 --clap $Clap --output $PortableRoot --allowed-output-root $Build | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Portable release staging failed with exit code $LASTEXITCODE" }

    Write-Host '=== PASS ==='
    Write-Host "VST3 artifact: $Vst3"
    Write-Host "CLAP artifact: $Clap"
    Write-Host "Portable checkpoint: $PortableRoot"
    Write-Host "Build log: $Log"
    Write-Host 'Nothing was installed automatically.'
}
catch {
    Write-Host '=== FAIL ==='
    Write-Host ("ERROR: {0}" -f $_.Exception.Message)
    throw
}
finally {
    Stop-Transcript | Out-Null
}
