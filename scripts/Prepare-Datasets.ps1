param(
    [switch]$Force,
    [switch]$SkipPythonDependencyInstall,
    [string]$Python = "python",
    [string]$IrcamSofa = "",
    [string]$KemarSofa = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Cache = Join-Path $Root ".cache\official-sofa"
$Out = Join-Path $Root "resources\datasets"
$Heads = Join-Path $Root "Heads"
New-Item -ItemType Directory -Force -Path $Cache, $Out, $Heads | Out-Null

function Ensure-PythonModules {
    # Keep the dependency probe quiet under Windows PowerShell 5.1. A failed
    # direct import writes a traceback to stderr, which can be promoted to a
    # terminating NativeCommandError while $ErrorActionPreference is Stop.
    & $Python -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('h5py') and importlib.util.find_spec('numpy') else 1)"
    if ($LASTEXITCODE -eq 0) { return }
    if ($SkipPythonDependencyInstall) {
        throw "Python modules h5py and numpy are required. Run: $Python -m pip install h5py numpy"
    }
    Write-Host "Installing build-only Python dependencies: h5py numpy"
    & $Python -m pip install h5py numpy
    if ($LASTEXITCODE -ne 0) { throw "Could not install h5py/numpy" }
    & $Python -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('h5py') and importlib.util.find_spec('numpy') else 1)"
    if ($LASTEXITCODE -ne 0) {
        throw "h5py/numpy installation completed but the selected Python cannot import them"
    }
}

function Test-SofaFile([string]$Path, [string]$Name) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "$Name was not found: $Path" }
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sig = New-Object byte[] 8
        if ($stream.Read($sig, 0, 8) -ne 8) { throw "$Name is too small" }
    } finally { $stream.Dispose() }
    $hdf = [byte[]](0x89,0x48,0x44,0x46,0x0d,0x0a,0x1a,0x0a)
    for ($i=0; $i -lt 8; $i++) {
        if ($sig[$i] -ne $hdf[$i]) { throw "$Name does not have the HDF5/SOFA signature" }
    }
    $hash = (Get-FileHash -Algorithm SHA256 $Path).Hash.ToLowerInvariant()
    Write-Host "  SHA-256 $hash"
    return (Resolve-Path -LiteralPath $Path).Path
}

function Get-OfficialSofa([string]$Url, [string]$Name, [string]$LocalPath) {
    $Path = Join-Path $Cache $Name
    if ($LocalPath) {
        Write-Host "Using supplied official dataset: $Name"
        $source = Test-SofaFile $LocalPath $Name
        if ($Force -or -not (Test-Path -LiteralPath $Path) -or
            ((Get-FileHash -Algorithm SHA256 $source).Hash -ne (Get-FileHash -Algorithm SHA256 $Path -ErrorAction SilentlyContinue).Hash)) {
            Copy-Item -LiteralPath $source -Destination $Path -Force
        }
    } elseif ($Force -or -not (Test-Path -LiteralPath $Path)) {
        Write-Host "Downloading official dataset: $Name"
        Invoke-WebRequest -Uri $Url -OutFile $Path -UseBasicParsing
    }
    return Test-SofaFile $Path $Name
}

Ensure-PythonModules

$ircamUrl = "https://sofacoustics.org/data/database/listen%20%28hrtf%29/IRC_1050_R_44100.sofa"
$kemarUrl = "https://sofacoustics.org/data/database/mit/mit_kemar_normal_pinna.sofa"

$ircam = Get-OfficialSofa $ircamUrl "IRC_1050_R_44100.sofa" $IrcamSofa
$kemar = Get-OfficialSofa $kemarUrl "mit_kemar_normal_pinna.sofa" $KemarSofa
$converter = Join-Path $Root "scripts\sofa_to_sthrtf.py"

Write-Host "Converting IRCAM LISTEN 1050 (SpaceTrace default)..."
& $Python $converter $ircam (Join-Path $Out "IRC_1050_R_44100.sthrtf") `
    --name "IRCAM LISTEN 1050" `
    --subject "1050" `
    --database "IRCAM LISTEN" `
    --source-url $ircamUrl `
    --licence-file (Join-Path $Root "third_party\LICENSE_IRCAM_LISTEN.txt") `
    --attribution "IRCAM LISTEN HRTF Database; Olivier Warusfel and the IRCAM Room Acoustics Team" `
    --minimum-phase-taps 128 `
    --processing-description "SpaceTrace default preparation: 128-tap minimum-phase HRIRs generated from the official IRCAM LISTEN 1050 SOFA data; onset and broadband interaural delay preserved on a quarter-sample grid; no historical game asset is redistributed" `
    --compensation (Join-Path $Root "resources\corrections\IRCAM1050_DatasetCorrected_ToneTrace.wav") `
    --compensation-name "SpaceTrace IRCAM 1050 Tonal Restoration" `
    --compensation-version "tonetrace-raw-v1"
if ($LASTEXITCODE -ne 0) { throw "IRCAM conversion failed" }

Write-Host "Converting MIT KEMAR normal pinna (alternative)..."
& $Python $converter $kemar (Join-Path $Out "mit_kemar_normal_pinna.sthrtf") `
    --name "MIT KEMAR Normal Pinna" `
    --subject "KEMAR normal pinna" `
    --database "MIT KEMAR" `
    --source-url $kemarUrl `
    --licence-file (Join-Path $Root "third_party\LICENSE_MIT_KEMAR.txt") `
    --attribution "Bill Gardner and Keith Martin, MIT Media Lab Machine Listening Group" `
    --processing-description "Raw MIT KEMAR Normal Pinna SOFA HRIRs with coordinates normalized to the SpaceTrace convention" `
    --compensation (Join-Path $Root "resources\corrections\MIT_KEMAR_DatasetCorrected_ToneTrace.wav") `
    --compensation-name "SpaceTrace KEMAR Tonal Restoration" `
    --compensation-version "tonetrace-raw-v1"
if ($LASTEXITCODE -ne 0) { throw "KEMAR conversion failed" }

Write-Host ""
Write-Host "Validating generated packages against the official SOFA sources..."
& $Python (Join-Path $Root "scripts\validate_builtins.py") `
    --ircam-sofa $ircam `
    --kemar-sofa $kemar `
    --ircam-package (Join-Path $Out "IRC_1050_R_44100.sthrtf") `
    --kemar-package (Join-Path $Out "mit_kemar_normal_pinna.sthrtf")
if ($LASTEXITCODE -ne 0) { throw "Built-in dataset validation failed" }

Write-Host ""
Write-Host "Preparing portable external head packages..."
$stripper = Join-Path $Root "scripts\strip_native_compensation.py"

function Write-PortableHead(
    [string]$FolderName,
    [string]$StableId,
    [string]$DisplayName,
    [string]$LegacyPackage,
    [string]$Correction,
    [string]$CorrectionName,
    [string]$CorrectionVersion,
    [string]$LicenseFile
) {
    $folder = Join-Path $Heads $FolderName
    New-Item -ItemType Directory -Force -Path $folder | Out-Null
    $portableHead = Join-Path $folder "head.sthrtf"
    & $Python $stripper $LegacyPackage $portableHead
    if ($LASTEXITCODE -ne 0) { throw "Could not create Raw-only portable head: $DisplayName" }
    Copy-Item -LiteralPath $Correction -Destination (Join-Path $folder "correction.wav") -Force
    Copy-Item -LiteralPath $LicenseFile -Destination (Join-Path $folder "LICENSE.txt") -Force
    $manifest = [ordered]@{
        schemaVersion = 1
        stableId = $StableId
        displayName = $DisplayName
        headFile = "head.sthrtf"
        headSha256 = (Get-FileHash -Algorithm SHA256 $portableHead).Hash.ToLowerInvariant()
        correctionFile = "correction.wav"
        correctionSha256 = (Get-FileHash -Algorithm SHA256 (Join-Path $folder "correction.wav")).Hash.ToLowerInvariant()
        correctionName = $CorrectionName
        correctionVersion = $CorrectionVersion
        levelTrimDb = 0.0
        leftGainDb = 0.0
        rightGainDb = 0.0
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $folder "manifest.json") -Encoding UTF8
    & $Python (Join-Path $Root "scripts\analyze_head_channels.py") $folder --write-manifest
    if ($LASTEXITCODE -ne 0) { throw "Could not derive front-center channel calibration: $DisplayName" }
}

Write-PortableHead `
    "IRCAM_1050" `
    "ircam.listen.1050.spacetrace-mp128.44100" `
    "IRCAM LISTEN 1050" `
    (Join-Path $Out "IRC_1050_R_44100.sthrtf") `
    (Join-Path $Root "resources\corrections\IRCAM1050_DatasetCorrected_ToneTrace.wav") `
    "SpaceTrace IRCAM 1050 Tonal Restoration" `
    "tonetrace-raw-v1" `
    (Join-Path $Root "third_party\LICENSE_IRCAM_LISTEN.txt")

Write-PortableHead `
    "MIT_KEMAR_Normal" `
    "mit.kemar.normal-pinna.44100" `
    "MIT KEMAR Normal Pinna" `
    (Join-Path $Out "mit_kemar_normal_pinna.sthrtf") `
    (Join-Path $Root "resources\corrections\MIT_KEMAR_DatasetCorrected_ToneTrace.wav") `
    "SpaceTrace KEMAR Tonal Restoration" `
    "tonetrace-raw-v1" `
    (Join-Path $Root "third_party\LICENSE_MIT_KEMAR.txt")

Write-Host "Built-in datasets and portable external head packages are ready."
