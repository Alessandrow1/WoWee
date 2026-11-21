# WOL updater v0.3: downloads an official release only after verification.
# Assets required: World-of-Legends-Windows.zip and World-of-Legends-Windows.zip.sha256
param([Parameter(Mandatory=$true)][string]$InstalledVersion, [int]$LauncherPid = 0, [switch]$Rollback)
$ErrorActionPreference = 'Stop'
$repo = 'Alessandrow1/WoWee'
$zipName = 'World-of-Legends-Windows.zip'
$hashName = "$zipName.sha256"
$api = "https://api.github.com/repos/$repo/releases/latest"
function StartInstaller([string]$mode,[string]$archive,[string]$checksum,[string]$tag) {
    $installer = Join-Path $PSScriptRoot 'world-of-legends-install.ps1'
    if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
        throw 'Instalador externo ausente. Recrie o pacote.'
    }
    # The installer must not run from the directory it will rename.
    $workerRoot = Join-Path $env:TEMP ('wol-installer-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $workerRoot | Out-Null
    $worker = Join-Path $workerRoot 'world-of-legends-install.ps1'
    Copy-Item -LiteralPath $installer -Destination $worker -Force

    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $worker +
        '" -Mode ' + $mode +
        ' -InstallDir "' + $PSScriptRoot + '"' +
        ' -LauncherPid ' + $LauncherPid +
        ' -UpdaterPid ' + $PID
    if ($mode -eq 'Install') {
        $arguments += ' -ArchivePath "' + $archive + '" -ChecksumPath "' + $checksum +
            '" -ExpectedVersion "' + $tag + '"'
    }
    Start-Process -FilePath 'powershell.exe' -ArgumentList $arguments -WorkingDirectory $workerRoot
    Write-Host 'Instalador iniciado em processo separado.'
}

function Notify([string]$message) {
    Write-Host $message
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show($message, 'World of Legends - Atualizacoes', 'OK', 'Information') | Out-Null
}
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $headers = @{ 'User-Agent' = 'WOL-Launcher'; 'Accept' = 'application/vnd.github+json' }
    if ($Rollback) {
        $backup = $PSScriptRoot + '.backup'
        if (-not (Test-Path -LiteralPath $backup -PathType Container)) {
            Notify 'Nao existe backup anterior para restaurar.'
            exit 0
        }
        $answer = Read-Host 'Restaurar a versao anterior? A versao atual sera preservada separadamente. [S/N]'
        if ($answer -notmatch '^(s|sim|y|yes)
    Write-Host 'Consultando GitHub Releases...'
    try {
        $release = Invoke-RestMethod -Uri $api -Headers $headers -TimeoutSec 20
    } catch {
        if ($_.Exception.Response -and [int]$_.Exception.Response.StatusCode -eq 404) {
            Notify 'Nenhuma release publica encontrada. Nada foi alterado.'
            exit 0
        }
        throw
    }
    if ($release.draft -or $release.prerelease) { throw 'Release nao estavel.' }
    $tag = [string]$release.tag_name
    if ($tag -notmatch '^v?(\d+)\.(\d+)\.(\d+)$') { throw "Tag invalida: $tag" }
    $latest = [version]$tag.TrimStart('v')
    $local = [version]$InstalledVersion.TrimStart('v')
    if ($latest -le $local) {
        Notify "Cliente atualizado: $local"
        exit 0
    }
    $assetZip = @($release.assets | Where-Object { $_.name -ceq $zipName })
    $assetHash = @($release.assets | Where-Object { $_.name -ceq $hashName })
    if ($assetZip.Count -ne 1 -or $assetHash.Count -ne 1) {
        throw "Release $tag sem os assets $zipName e $hashName."
    }
    $prefix = "https://github.com/$repo/releases/download/"
    foreach ($asset in @($assetZip[0], $assetHash[0])) {
        if (-not ([string]$asset.browser_download_url).StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Origem nao confiavel. Download bloqueado.'
        }
    }
    $answer = Read-Host "Release $latest disponivel. Baixar e verificar? [S/N]"
    if ($answer -notmatch '^(s|sim|y|yes)$') {
        Notify 'Download cancelado. Nada foi alterado.'
        exit 0
    }
    $target = Join-Path (Join-Path $PSScriptRoot 'updates') ("v" + $latest.ToString())
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    $zipPath = Join-Path $target $zipName
    $hashPath = Join-Path $target $hashName
    $tempZip = "$zipPath.part"
    $tempHash = "$hashPath.part"
    Remove-Item -LiteralPath $tempZip, $tempHash -ErrorAction SilentlyContinue
    try {
        Write-Host 'Baixando checksum...'
        Invoke-WebRequest -UseBasicParsing -Uri $assetHash[0].browser_download_url -Headers $headers -OutFile $tempHash -TimeoutSec 60
        $hashText = (Get-Content -LiteralPath $tempHash -Raw).Trim()
        $pattern = '(?i)^([a-f0-9]{64})\s+\*?' + [regex]::Escape($zipName) + '$'
        if ($hashText -notmatch $pattern) { throw 'Manifesto SHA256 invalido.' }
        $expected = $Matches[1].ToUpperInvariant()
        Write-Host 'Baixando pacote...'
        Invoke-WebRequest -UseBasicParsing -Uri $assetZip[0].browser_download_url -Headers $headers -OutFile $tempZip -TimeoutSec 1800
        $actual = (Get-FileHash -LiteralPath $tempZip -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($actual -ne $expected) { throw 'Falha SHA256. Pacote descartado.' }
        Move-Item -LiteralPath $tempHash -Destination $hashPath -Force
        Move-Item -LiteralPath $tempZip -Destination $zipPath -Force
    } catch {
        Remove-Item -LiteralPath $tempHash, $tempZip -ErrorAction SilentlyContinue
        throw
    }
    Notify ("SHA256 confirmado. Download salvo em: " + $zipPath + [Environment]::NewLine +
        'Pode instalar com backup e restaurar a versao anterior pelo launcher.')
    $installAnswer = Read-Host 'Instalar agora? O launcher sera fechado. Feche o jogo primeiro. [S/N]'
    if ($installAnswer -match '^(s|sim|y|yes)
} catch {
    $message = 'Nao foi possivel atualizar: ' + $_.Exception.Message + [Environment]::NewLine +
        'A instalacao atual permanece intacta.'
    Write-Host $message -ForegroundColor Red
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show($message, 'World of Legends - Erro', 'OK', 'Error') | Out-Null
    exit 1
}
) { Notify 'Restauracao cancelada.'; exit 0 }
        StartInstaller 'Rollback' '' '' ''
        exit 0
    }
    Write-Host 'Consultando GitHub Releases...'
    try {
        $release = Invoke-RestMethod -Uri $api -Headers $headers -TimeoutSec 20
    } catch {
        if ($_.Exception.Response -and [int]$_.Exception.Response.StatusCode -eq 404) {
            Notify 'Nenhuma release publica encontrada. Nada foi alterado.'
            exit 0
        }
        throw
    }
    if ($release.draft -or $release.prerelease) { throw 'Release nao estavel.' }
    $tag = [string]$release.tag_name
    if ($tag -notmatch '^v?(\d+)\.(\d+)\.(\d+)$') { throw "Tag invalida: $tag" }
    $latest = [version]$tag.TrimStart('v')
    $local = [version]$InstalledVersion.TrimStart('v')
    if ($latest -le $local) {
        Notify "Cliente atualizado: $local"
        exit 0
    }
    $assetZip = @($release.assets | Where-Object { $_.name -ceq $zipName })
    $assetHash = @($release.assets | Where-Object { $_.name -ceq $hashName })
    if ($assetZip.Count -ne 1 -or $assetHash.Count -ne 1) {
        throw "Release $tag sem os assets $zipName e $hashName."
    }
    $prefix = "https://github.com/$repo/releases/download/"
    foreach ($asset in @($assetZip[0], $assetHash[0])) {
        if (-not ([string]$asset.browser_download_url).StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Origem nao confiavel. Download bloqueado.'
        }
    }
    $answer = Read-Host "Release $latest disponivel. Baixar e verificar? [S/N]"
    if ($answer -notmatch '^(s|sim|y|yes)$') {
        Notify 'Download cancelado. Nada foi alterado.'
        exit 0
    }
    $target = Join-Path (Join-Path $PSScriptRoot 'updates') ("v" + $latest.ToString())
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    $zipPath = Join-Path $target $zipName
    $hashPath = Join-Path $target $hashName
    $tempZip = "$zipPath.part"
    $tempHash = "$hashPath.part"
    Remove-Item -LiteralPath $tempZip, $tempHash -ErrorAction SilentlyContinue
    try {
        Write-Host 'Baixando checksum...'
        Invoke-WebRequest -UseBasicParsing -Uri $assetHash[0].browser_download_url -Headers $headers -OutFile $tempHash -TimeoutSec 60
        $hashText = (Get-Content -LiteralPath $tempHash -Raw).Trim()
        $pattern = '(?i)^([a-f0-9]{64})\s+\*?' + [regex]::Escape($zipName) + '$'
        if ($hashText -notmatch $pattern) { throw 'Manifesto SHA256 invalido.' }
        $expected = $Matches[1].ToUpperInvariant()
        Write-Host 'Baixando pacote...'
        Invoke-WebRequest -UseBasicParsing -Uri $assetZip[0].browser_download_url -Headers $headers -OutFile $tempZip -TimeoutSec 1800
        $actual = (Get-FileHash -LiteralPath $tempZip -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($actual -ne $expected) { throw 'Falha SHA256. Pacote descartado.' }
        Move-Item -LiteralPath $tempHash -Destination $hashPath -Force
        Move-Item -LiteralPath $tempZip -Destination $zipPath -Force
    } catch {
        Remove-Item -LiteralPath $tempHash, $tempZip -ErrorAction SilentlyContinue
        throw
    }
    Notify ("SHA256 confirmado. Download salvo em: " + $zipPath + [Environment]::NewLine +
        'O jogo NAO foi substituido. Instalacao e rollback serao implementados depois.')
} catch {
    $message = 'Nao foi possivel atualizar: ' + $_.Exception.Message + [Environment]::NewLine +
        'A instalacao atual permanece intacta.'
    Write-Host $message -ForegroundColor Red
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show($message, 'World of Legends - Erro', 'OK', 'Error') | Out-Null
    exit 1
}
) {
        StartInstaller 'Install' $zipPath $hashPath $tag
        exit 0
    }
    Notify 'Pacote verificado e salvo. Voce pode instalar quando quiser verificando atualizacoes novamente.'
} catch {
    $message = 'Nao foi possivel atualizar: ' + $_.Exception.Message + [Environment]::NewLine +
        'A instalacao atual permanece intacta.'
    Write-Host $message -ForegroundColor Red
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show($message, 'World of Legends - Erro', 'OK', 'Error') | Out-Null
    exit 1
}
) { Notify 'Restauracao cancelada.'; exit 0 }
        StartInstaller 'Rollback' '' '' ''
        exit 0
    }
    Write-Host 'Consultando GitHub Releases...'
    try {
        $release = Invoke-RestMethod -Uri $api -Headers $headers -TimeoutSec 20
    } catch {
        if ($_.Exception.Response -and [int]$_.Exception.Response.StatusCode -eq 404) {
            Notify 'Nenhuma release publica encontrada. Nada foi alterado.'
            exit 0
        }
        throw
    }
    if ($release.draft -or $release.prerelease) { throw 'Release nao estavel.' }
    $tag = [string]$release.tag_name
    if ($tag -notmatch '^v?(\d+)\.(\d+)\.(\d+)$') { throw "Tag invalida: $tag" }
    $latest = [version]$tag.TrimStart('v')
    $local = [version]$InstalledVersion.TrimStart('v')
    if ($latest -le $local) {
        Notify "Cliente atualizado: $local"
        exit 0
    }
    $assetZip = @($release.assets | Where-Object { $_.name -ceq $zipName })
    $assetHash = @($release.assets | Where-Object { $_.name -ceq $hashName })
    if ($assetZip.Count -ne 1 -or $assetHash.Count -ne 1) {
        throw "Release $tag sem os assets $zipName e $hashName."
    }
    $prefix = "https://github.com/$repo/releases/download/"
    foreach ($asset in @($assetZip[0], $assetHash[0])) {
        if (-not ([string]$asset.browser_download_url).StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Origem nao confiavel. Download bloqueado.'
        }
    }
    $answer = Read-Host "Release $latest disponivel. Baixar e verificar? [S/N]"
    if ($answer -notmatch '^(s|sim|y|yes)$') {
        Notify 'Download cancelado. Nada foi alterado.'
        exit 0
    }
    $target = Join-Path (Join-Path $PSScriptRoot 'updates') ("v" + $latest.ToString())
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    $zipPath = Join-Path $target $zipName
    $hashPath = Join-Path $target $hashName
    $tempZip = "$zipPath.part"
    $tempHash = "$hashPath.part"
    Remove-Item -LiteralPath $tempZip, $tempHash -ErrorAction SilentlyContinue
    try {
        Write-Host 'Baixando checksum...'
        Invoke-WebRequest -UseBasicParsing -Uri $assetHash[0].browser_download_url -Headers $headers -OutFile $tempHash -TimeoutSec 60
        $hashText = (Get-Content -LiteralPath $tempHash -Raw).Trim()
        $pattern = '(?i)^([a-f0-9]{64})\s+\*?' + [regex]::Escape($zipName) + '$'
        if ($hashText -notmatch $pattern) { throw 'Manifesto SHA256 invalido.' }
        $expected = $Matches[1].ToUpperInvariant()
        Write-Host 'Baixando pacote...'
        Invoke-WebRequest -UseBasicParsing -Uri $assetZip[0].browser_download_url -Headers $headers -OutFile $tempZip -TimeoutSec 1800
        $actual = (Get-FileHash -LiteralPath $tempZip -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($actual -ne $expected) { throw 'Falha SHA256. Pacote descartado.' }
        Move-Item -LiteralPath $tempHash -Destination $hashPath -Force
        Move-Item -LiteralPath $tempZip -Destination $zipPath -Force
    } catch {
        Remove-Item -LiteralPath $tempHash, $tempZip -ErrorAction SilentlyContinue
        throw
    }
    Notify ("SHA256 confirmado. Download salvo em: " + $zipPath + [Environment]::NewLine +
        'O jogo NAO foi substituido. Instalacao e rollback serao implementados depois.')
} catch {
    $message = 'Nao foi possivel atualizar: ' + $_.Exception.Message + [Environment]::NewLine +
        'A instalacao atual permanece intacta.'
    Write-Host $message -ForegroundColor Red
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show($message, 'World of Legends - Erro', 'OK', 'Error') | Out-Null
    exit 1
}
