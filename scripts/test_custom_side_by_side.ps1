param([Parameter(Mandatory = $true)][string]$Installer)

$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true') {
    throw 'This synthetic installer test is restricted to disposable GitHub Actions runners.'
}
$Installer = (Resolve-Path $Installer).Path
$target = Join-Path $env:LOCALAPPDATA 'Programs\Unlayered Custom Beta'
$betaInstall = Join-Path $env:ProgramFiles 'OrcaSlicer BambuConnect'
$betaData = Join-Path $env:APPDATA 'OrcaSlicer'
$betaKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\OrcaSlicer BambuConnect'
$customKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\UnlayeredCustomBeta'

# Refuse to replace any pre-existing installation, profile, or registry entry.
foreach ($path in @($target, $betaInstall, $betaData, $betaKey, $customKey)) {
    if (Test-Path $path) { throw "Test needs a clean disposable runner; already exists: $path" }
}
$sentinel = [guid]::NewGuid().ToString()
try {
    New-Item -ItemType Directory -Path $betaInstall, $betaData -Force | Out-Null
    Set-Content (Join-Path $betaInstall 'beta5-install-sentinel.txt') $sentinel
    Set-Content (Join-Path $betaData 'beta5-profile-sentinel.txt') $sentinel
    New-Item $betaKey -Force | Out-Null
    New-ItemProperty $betaKey -Name DisplayName -Value $sentinel | Out-Null

    function Assert-Beta5Unchanged {
        foreach ($path in @((Join-Path $betaInstall 'beta5-install-sentinel.txt'),
                            (Join-Path $betaData 'beta5-profile-sentinel.txt'))) {
            if ((Get-Content $path -Raw).Trim() -ne $sentinel) { throw "Beta 5 sentinel changed: $path" }
        }
        if ((Get-ItemProperty $betaKey).DisplayName -ne $sentinel) { throw 'Beta 5 uninstall registry entry changed' }
    }

    # /D must be the last NSIS argument and must not have quotes of its own.
    $process = Start-Process $Installer -ArgumentList "/S /D=$target" -PassThru
    if (-not $process.WaitForExit(180000)) { $process.Kill(); throw 'Installer timed out' }
    if ($process.ExitCode -ne 0) { throw "Installer exited with $($process.ExitCode)" }
    Assert-Beta5Unchanged
    $exe = Join-Path $target 'unlayered-slicer.exe'
    if (-not (Test-Path $exe)) { throw 'Custom executable was not installed' }
    if ((Get-Item $exe).VersionInfo.ProductName -ne 'Unlayered Custom Beta') {
        throw 'Installed executable has the wrong product identity'
    }
    if (-not (Test-Path $customKey)) { throw 'Separate custom uninstall entry is missing' }

    $uninstaller = Join-Path $target 'Uninstall.exe'
    if (-not (Test-Path $uninstaller)) { throw 'Custom uninstaller is missing' }
    $process = Start-Process $uninstaller -ArgumentList "/S _?=$target" -PassThru
    if (-not $process.WaitForExit(180000)) { $process.Kill(); throw 'Uninstaller timed out' }
    if ($process.ExitCode -ne 0) { throw "Uninstaller exited with $($process.ExitCode)" }
    Assert-Beta5Unchanged
    if (Test-Path $exe) { throw 'Custom executable remained after uninstall' }
    if (Test-Path $customKey) { throw 'Custom uninstall entry remained after uninstall' }
    Write-Output 'PASS: synthetic Beta 5 install/profile/registry sentinels survived custom install and uninstall.'
    Write-Output 'This does not verify an existing real Beta 5 installation or interactive GUI behavior.'
} finally {
    # These exact paths were verified absent before creating the test sentinels.
    Remove-Item $betaInstall, $betaData, $betaKey -Recurse -Force -ErrorAction SilentlyContinue
}
