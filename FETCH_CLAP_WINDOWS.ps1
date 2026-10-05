$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Deps = Join-Path $Root 'deps'
$Dest = Join-Path $Deps 'clap-juce-extensions'
$Archives = Join-Path $Deps 'archives'
# These commits were checked against the wrapper's actual Git submodule tree.
$WrapperCommit = '55525c9858d4b25687be7759a5e0f70eccef218e'
$ClapCommit = '29ffcc273be7c7c651f6c9953b99e69700e2387a'
$HelpersCommit = 'a61bcdf0ecc2c8db1e80bfe8bf9cb7e8d9fd2bbc'
$Pins = @(
    @{ Name='clap-juce-extensions'; Commit=$WrapperCommit; Hash='a6ff539a7faebcfcda4131d8982a388dd3382350d082511dfd9578ce7358f9ea'; Target=$Dest },
    @{ Name='clap'; Commit=$ClapCommit; Hash='98f18381998028d751669c218265dbba4a17e44bb112e63a8ff02d07aee62d76'; Target=(Join-Path $Dest 'clap-libs\clap') },
    @{ Name='clap-helpers'; Commit=$HelpersCommit; Hash='0369451cdb2f82eff5aef1db5abff6bff300c553da0dd267433e386bf97fd674'; Target=(Join-Path $Dest 'clap-libs\clap-helpers') }
)
New-Item -ItemType Directory -Force -Path $Archives | Out-Null
foreach ($Pin in $Pins) {
    $ArchivePath = Join-Path $Archives ($Pin.Name + '-' + $Pin.Commit + '.zip')
    if (-not (Test-Path -LiteralPath $ArchivePath)) {
        $Url = 'https://codeload.github.com/free-audio/{0}/zip/{1}' -f $Pin.Name, $Pin.Commit
        $Partial = $ArchivePath + '.partial'
        Write-Host "Downloading $Url"
        Invoke-WebRequest -Uri $Url -OutFile $Partial -UseBasicParsing
        $Actual = (Get-FileHash -LiteralPath $Partial -Algorithm SHA256).Hash
        if ($Actual -ne $Pin.Hash) { throw "SHA-256 mismatch for $Partial; existing dependencies were preserved." }
        Move-Item -LiteralPath $Partial -Destination $ArchivePath
    }
    if ((Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash -ne $Pin.Hash) {
        throw "Cached archive SHA-256 mismatch: $ArchivePath"
    }
    $TargetRoot = [System.IO.Path]::GetFullPath($Pin.Target).TrimEnd('\') + '\'
    $Zip = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $Prefix = $Pin.Name + '-' + $Pin.Commit + '/'
        foreach ($Entry in $Zip.Entries) {
            if ($Entry.Name -eq '') { continue }
            if (-not $Entry.FullName.StartsWith($Prefix, [System.StringComparison]::Ordinal)) { throw 'Unexpected archive root.' }
            $Relative = $Entry.FullName.Substring($Prefix.Length).Replace('/', '\')
            $Target = [System.IO.Path]::GetFullPath((Join-Path $Pin.Target $Relative))
            if (-not $Target.StartsWith($TargetRoot, [System.StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe archive path.' }
            $Stream = $Entry.Open()
            try {
                if (Test-Path -LiteralPath $Target) {
                    $Sha = [System.Security.Cryptography.SHA256]::Create()
                    try { $Expected = [BitConverter]::ToString($Sha.ComputeHash($Stream)).Replace('-', '') }
                    finally { $Sha.Dispose() }
                    if ((Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash -ne $Expected) {
                        throw "Local dependency differs from the pinned archive: $Target. Local content was preserved."
                    }
                } else {
                    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Target) | Out-Null
                    $Output = [System.IO.File]::Create($Target)
                    try { $Stream.CopyTo($Output) } finally { $Output.Dispose() }
                }
            } finally { $Stream.Dispose() }
        }
    } finally { $Zip.Dispose() }
    Write-Host ("Verified cached {0} at {1}" -f $Pin.Name, $Pin.Commit)
}
$Pins | ForEach-Object { $_.Name + ' ' + $_.Commit } |
    Set-Content -LiteralPath (Join-Path $Dest 'SPACETRACE_PINNED_REVISIONS.txt') -Encoding ASCII
Write-Host "Pinned CLAP dependencies ready: $Dest"
