param(
    [switch]$Force,
    [switch]$SkipPythonDependencyInstall,
    [string]$Python = "python",
    [string]$SadieSofa = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Cache = Join-Path $Root ".cache\official-sofa\sadie2-d1"
$Out = Join-Path $Root "resources\datasets"
$HeadOut = Join-Path $Root "Heads\KU100_SADIE_D1"
$Archive = Join-Path $Cache "D1_HRIR_SOFA.zip"
$Extracted = Join-Path $Cache "extracted"
$ArchiveUrl = "https://zenodo.org/records/12092466/files/D1_HRIR_SOFA.zip?download=1"
$ExpectedArchiveMd5 = "4850c1eb8e63e2d4f605edcdb4d5c883"
$Correction = Join-Path $Root "resources\corrections\SADIE2_D1_KU100_DatasetCorrected_ToneTrace.wav"
New-Item -ItemType Directory -Force -Path $Cache, $Out, $HeadOut | Out-Null

function Ensure-PythonModules {
    # Do not probe by importing the modules directly.  On Windows PowerShell
    # 5.1 a failed Python import writes a traceback to stderr, which can be
    # promoted to NativeCommandError while $ErrorActionPreference is Stop and
    # abort the build before we get a chance to install the missing modules.
    # importlib.util.find_spec gives us a quiet exit-code-only probe instead.
    & $Python -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('h5py') and importlib.util.find_spec('numpy') else 1)"
    $probeExit = $LASTEXITCODE
    if ($probeExit -eq 0) { return }

    if ($SkipPythonDependencyInstall) {
        throw "Python modules h5py and numpy are required. Run: $Python -m pip install h5py numpy"
    }

    Write-Host "Installing build-only Python dependencies: h5py numpy"
    & $Python -m pip install h5py numpy
    if ($LASTEXITCODE -ne 0) { throw "Could not install h5py/numpy" }

    # Verify the installation with the same quiet probe so a packaging/path
    # mismatch fails with a concise build error rather than an import traceback.
    & $Python -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('h5py') and importlib.util.find_spec('numpy') else 1)"
    if ($LASTEXITCODE -ne 0) {
        throw "h5py/numpy installation completed but the selected Python cannot import them"
    }
}

function Test-SofaFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "SADIE II D1 SOFA was not found: $Path" }
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sig = New-Object byte[] 8
        if ($stream.Read($sig, 0, 8) -ne 8) { throw "SADIE II D1 SOFA is too small" }
    } finally { $stream.Dispose() }
    $hdf = [byte[]](0x89,0x48,0x44,0x46,0x0d,0x0a,0x1a,0x0a)
    for ($i=0; $i -lt 8; $i++) {
        if ($sig[$i] -ne $hdf[$i]) { throw "SADIE II D1 file does not have the HDF5/SOFA signature" }
    }
    $hash = (Get-FileHash -Algorithm SHA256 $Path).Hash.ToLowerInvariant()
    Write-Host "SADIE II D1 SOFA SHA-256: $hash"
    return (Resolve-Path -LiteralPath $Path).Path
}

if (-not (Test-Path -LiteralPath $Correction)) { throw "KU100 Tone Trace correction asset is missing: $Correction" }

Ensure-PythonModules

if ($SadieSofa) {
    Write-Host "Using supplied SADIE II D1 SOFA: $SadieSofa"
    $sofa = Test-SofaFile $SadieSofa
} else {
    if ($Force -or -not (Test-Path -LiteralPath $Archive)) {
        Write-Host "Downloading official SADIE II D1 HRIR SOFA archive from Zenodo..."
        Invoke-WebRequest -Uri $ArchiveUrl -OutFile $Archive -UseBasicParsing
    }
    $archiveMd5 = (Get-FileHash -Algorithm MD5 $Archive).Hash.ToLowerInvariant()
    if ($archiveMd5 -ne $ExpectedArchiveMd5) {
        throw "SADIE II D1 archive MD5 mismatch. Expected $ExpectedArchiveMd5, got $archiveMd5"
    }
    Write-Host "SADIE II D1 archive MD5 verified: $archiveMd5"
    if ($Force -and (Test-Path -LiteralPath $Extracted)) { Remove-Item $Extracted -Recurse -Force }
    if (-not (Test-Path -LiteralPath $Extracted)) {
        Expand-Archive -LiteralPath $Archive -DestinationPath $Extracted -Force
    }
    $selector = Join-Path $Root "scripts\select_sadie_d1_sofa.py"
    $selected = (& $Python $selector $Extracted | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $selected) {
        throw "Could not identify the intended SADIE II D1 44.1 kHz / 256-tap SOFA inside the official archive"
    }
    $sofa = Test-SofaFile $selected
}

$converter = Join-Path $Root "scripts\sofa_to_sthrtf.py"
$output = Join-Path $Out "sadie2_d1_ku100_44100.sthrtf"
Write-Host "Converting SADIE II D1 / Neumann KU100 with the measured Tone Trace global restoration..."
& $Python $converter $sofa $output `
    --name "SADIE II D1 / Neumann KU100" `
    --subject "D1 (Neumann KU100)" `
    --database "SADIE II" `
    --source-url $ArchiveUrl `
    --licence-file (Join-Path $Root "third_party\LICENSE_SADIE_II.txt") `
    --attribution "SADIE II Database, University of York AudioLab; subject D1, Neumann KU100; Armstrong, Thresh and Kearney" `
    --compensation $Correction `
    --compensation-name "Tone Trace raw-HRTF global restoration" `
    --compensation-version "tonetrace-raw-v1" `
    --processing-description "Raw SADIE II D1 KU100 44.1 kHz 256-tap SOFA HRIRs; source coordinates normalized to the SpaceTrace convention; fixed Tone Trace global tonal restoration stored non-destructively and applied identically to both ears"
if ($LASTEXITCODE -ne 0) { throw "SADIE II D1 conversion failed" }

Write-Host "Validating generated SADIE II D1 / KU100 package against the official SOFA..."
& $Python (Join-Path $Root "scripts\validate_builtins.py") `
    --ku100-sofa $sofa `
    --ku100-package $output `
    --ku100-compensation $Correction
if ($LASTEXITCODE -ne 0) { throw "SADIE II D1 package validation failed" }
$packageSha256 = (Get-FileHash -Algorithm SHA256 $output).Hash.ToLowerInvariant()
Write-Host "SADIE II D1 native package SHA-256: $packageSha256"

Write-Host "SADIE II D1 native package ready: $output"


$portableHead = Join-Path $HeadOut "head.sthrtf"
& $Python (Join-Path $Root "scripts\strip_native_compensation.py") $output $portableHead
if ($LASTEXITCODE -ne 0) { throw "Could not create Raw-only portable KU100 head package" }
Copy-Item -LiteralPath $Correction -Destination (Join-Path $HeadOut "correction.wav") -Force
$packageSha256 = (Get-FileHash -Algorithm SHA256 $portableHead).Hash.ToLowerInvariant()
$correctionSha256 = (Get-FileHash -Algorithm SHA256 (Join-Path $HeadOut "correction.wav")).Hash.ToLowerInvariant()
$manifest = [ordered]@{
    schemaVersion = 1
    stableId = "sadie2.d1.ku100.44100"
    displayName = "SADIE II D1 / Neumann KU100"
    headFile = "head.sthrtf"
    headSha256 = $packageSha256
    correctionFile = "correction.wav"
    correctionSha256 = $correctionSha256
    correctionName = "Tone Trace raw-HRTF global restoration"
    correctionVersion = "tonetrace-raw-v1"
    levelTrimDb = 0.0
    leftGainDb = 0.0
    rightGainDb = 0.0
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $HeadOut "manifest.json") -Encoding UTF8
& $Python (Join-Path $Root "scripts\analyze_head_channels.py") $HeadOut --write-manifest
if ($LASTEXITCODE -ne 0) { throw "Could not derive front-center channel calibration for SADIE II D1 / KU100" }
Copy-Item -LiteralPath (Join-Path $Root "third_party\LICENSE_SADIE_II.txt") -Destination (Join-Path $HeadOut "LICENSE.txt") -Force
Write-Host "Portable KU100 head package ready: $HeadOut"
