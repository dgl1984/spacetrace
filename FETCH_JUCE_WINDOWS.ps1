$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Deps = Join-Path $Root 'deps'
$JucePath = Join-Path $Deps 'JUCE'
$Version = '8.0.12'
$ArchiveUrl = "https://github.com/juce-framework/JUCE/releases/download/$Version/juce-$Version-windows.zip"
$ExpectedSha256 = '15b4ed8302127138dfee98e61a2b5ae1481b39649b139e02e77bfc29462b7efb'
$ArchivePath = Join-Path $Deps "juce-$Version-windows.zip"
$ExtractPath = Join-Path $Deps '_juce_extract'

New-Item -ItemType Directory -Force -Path $Deps | Out-Null
if (Test-Path $ArchivePath) { Remove-Item $ArchivePath -Force }
if (Test-Path $ExtractPath) { Remove-Item $ExtractPath -Recurse -Force }
if (Test-Path $JucePath) { Remove-Item $JucePath -Recurse -Force }

Write-Host "Downloading JUCE $Version..."
Invoke-WebRequest -UseBasicParsing -Uri $ArchiveUrl -OutFile $ArchivePath
$ActualSha256 = (Get-FileHash -Algorithm SHA256 $ArchivePath).Hash.ToLowerInvariant()
if ($ActualSha256 -ne $ExpectedSha256) {
    throw "JUCE archive SHA-256 mismatch. Expected $ExpectedSha256, got $ActualSha256"
}

Expand-Archive -Path $ArchivePath -DestinationPath $ExtractPath -Force
$Candidate = Get-ChildItem -Path $ExtractPath -Filter CMakeLists.txt -File -Recurse |
    Where-Object { Test-Path (Join-Path $_.Directory.FullName 'modules') } |
    Select-Object -First 1
if (-not $Candidate) { throw 'Could not locate the JUCE source root after extraction.' }
Move-Item -Path $Candidate.Directory.FullName -Destination $JucePath

if (-not (Test-Path (Join-Path $JucePath 'CMakeLists.txt'))) {
    throw 'JUCE extraction did not produce deps\JUCE\CMakeLists.txt.'
}

Remove-Item $ArchivePath -Force
Remove-Item $ExtractPath -Recurse -Force
Write-Host "JUCE $Version ready at $JucePath"
