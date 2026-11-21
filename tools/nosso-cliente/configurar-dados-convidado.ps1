param(
    [Parameter(Mandatory = $true)][string]$LauncherDir
)
$ErrorActionPreference = 'Stop'

# Configura apenas os dados que o jogador ja possui; nao baixa nem distribui
# recursos proprietarios do World of Warcraft.
Add-Type -AssemblyName System.Windows.Forms

$launcherDir = [IO.Path]::GetFullPath($LauncherDir)
$ini = Join-Path $launcherDir 'World-of-Legends.ini'
if (-not (Test-Path -LiteralPath $ini -PathType Leaf)) {
    [System.Windows.Forms.MessageBox]::Show(
        'Arquivo World-of-Legends.ini nao encontrado ao lado do launcher.',
        'World of Legends', 'OK', 'Error') | Out-Null
    exit 1
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WolIni {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern uint GetPrivateProfileString(
        string section, string key, string fallback, System.Text.StringBuilder result, uint size, string file);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    public static extern bool WritePrivateProfileString(
        string section, string key, string value, string file);
}
'@

function ValidDataRoot([string]$path) {
    if ([string]::IsNullOrWhiteSpace($path)) { return $false }
    try {
        $resolved = [IO.Path]::GetFullPath($path)
        return (Test-Path -LiteralPath (Join-Path $resolved 'expansions\wotlk') -PathType Container)
    } catch { return $false }
}

function GetConfiguredRoot {
    $buffer = New-Object System.Text.StringBuilder 4096
    [void][WolIni]::GetPrivateProfileString('WorldOfLegends', 'DataPath', '', $buffer, 4096, $ini)
    return $buffer.ToString().Trim()
}

function Probe([string]$candidate) {
    if (ValidDataRoot $candidate) { return [IO.Path]::GetFullPath($candidate) }
    return $null
}

try {
    $found = Probe (GetConfiguredRoot)
    if (-not $found) {
        $candidates = New-Object System.Collections.Generic.List[string]
        $candidates.Add($launcherDir)
        $candidates.Add((Split-Path -Parent $launcherDir))
        foreach ($base in @($env:USERPROFILE, $env:PUBLIC, $env:ProgramFiles,
            ${env:ProgramFiles(x86)}, $env:ProgramData)) {
            if ([string]::IsNullOrWhiteSpace($base)) { continue }
            $candidates.Add($base)
            foreach ($name in @('Desktop','Documents','Downloads','Games','World of Warcraft',
                'World of Legends','WoW','WoW 3.3.5a','WotLK')) {
                $candidates.Add((Join-Path $base $name))
            }
        }
        foreach ($drive in [IO.DriveInfo]::GetDrives()) {
            if (-not $drive.IsReady -or $drive.DriveType -notin @('Fixed','Removable')) { continue }
            $root = $drive.RootDirectory.FullName
            $candidates.Add($root)
            foreach ($name in @('Games','Jogos','World of Warcraft','World of Legends','WoW',
                'WoW 3.3.5a','WotLK','SteamLibrary','Program Files')) {
                $candidates.Add((Join-Path $root $name))
            }
        }
        foreach ($candidate in $candidates) {
            $found = Probe $candidate
            if ($found) { break }
        }
        # Pastas de jogos frequentemente sao um nivel abaixo de Games ou Downloads.
        if (-not $found) {
            foreach ($parent in $candidates) {
                if (-not (Test-Path -LiteralPath $parent -PathType Container)) { continue }
                try {
                    $children = @(Get-ChildItem -LiteralPath $parent -Directory -ErrorAction Stop |
                        Select-Object -First 80)
                    foreach ($child in $children) {
                        $found = Probe $child.FullName
                        if ($found) { break }
                    }
                    if ($found) { break }
                } catch { continue }
            }
        }
    }
    if (-not $found) {
        [System.Windows.Forms.MessageBox]::Show(
            "Nao localizamos automaticamente expansions\wotlk. Selecione a pasta principal de uma instalacao legitima que contenha expansions\wotlk. O instalador pode copiar os dados para esta pasta, se voce quiser.",
            'World of Legends - localizar dados', 'OK', 'Information') | Out-Null
        $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
        $dialog.Description = 'Selecione a pasta que CONTEM expansions\wotlk'
        $dialog.ShowNewFolderButton = $false
        if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
            $found = Probe $dialog.SelectedPath
        }
    }
    if (-not $found) {
        [System.Windows.Forms.MessageBox]::Show(
            "Dados nao encontrados. E necessario possuir uma pasta expansions\wotlk compativel. Nao foi feita nenhuma alteracao.",
            'World of Legends', 'OK', 'Warning') | Out-Null
        exit 1
    }
    # Opcao local: tornar a instalacao independente sem distribuir/copiar
    # material de terceiros no ZIP publicado. O proprio jogador escolhe
    # a origem dos dados que tem direito de utilizar no seu PC.
    $portableRoot = $launcherDir
    $portableData = Join-Path $portableRoot 'expansions\wotlk'
    if (-not (ValidDataRoot $portableRoot) -and
        -not [string]::Equals([IO.Path]::GetFullPath($found).TrimEnd('\'),
                              [IO.Path]::GetFullPath($portableRoot).TrimEnd('\'),
                              [StringComparison]::OrdinalIgnoreCase)) {
        $choice = [System.Windows.Forms.MessageBox]::Show(
            "Dados encontrados em:`n$found`n`nDeseja COPIAR esses dados para a pasta do World of Legends?`n`nIsso criara uma instalacao independente, mas podera consumir muitos GB e demorar. Os originais serao preservados.`n`nSIM: copiar. NAO: usar os arquivos onde ja estao.",
            'World of Legends - instalacao portatil',
            [System.Windows.Forms.MessageBoxButtons]::YesNo,
            [System.Windows.Forms.MessageBoxIcon]::Question)
        if ($choice -eq [System.Windows.Forms.DialogResult]::Yes) {
            $sourceData = Join-Path $found 'expansions\wotlk'
            if (-not (Test-Path -LiteralPath $sourceData -PathType Container)) {
                throw 'Origem WotLK nao encontrada.'
            }
            $srcCanonical = [IO.Path]::GetFullPath($sourceData).TrimEnd('\')
            $dstCanonical = [IO.Path]::GetFullPath($portableData).TrimEnd('\')
            if ($dstCanonical.StartsWith($srcCanonical + '\', [StringComparison]::OrdinalIgnoreCase) -or
                $srcCanonical.StartsWith($dstCanonical + '\', [StringComparison]::OrdinalIgnoreCase)) {
                throw 'Copiar uma pasta para dentro dela mesma nao e permitido.'
            }
            if (Test-Path -LiteralPath $portableData) {
                throw 'Ja existe uma pasta expansions\\wotlk local. Resolva isso antes de copiar.'
            }
            $driveRoot = [IO.Path]::GetPathRoot($launcherDir)
            $drive = New-Object IO.DriveInfo($driveRoot)
            $size = [long]0
            Get-ChildItem -LiteralPath $sourceData -File -Recurse -ErrorAction Stop |
                ForEach-Object { $size += $_.Length }
            if ($drive.AvailableFreeSpace -lt ($size + 512MB)) {
                throw ('Espaco insuficiente: sao necessarios aproximadamente ' +
                    [math]::Ceiling(($size + 512MB) / 1GB) + ' GB livres.')
            }
            $expansionFolder = Join-Path $portableRoot 'expansions'
            [IO.Directory]::CreateDirectory($expansionFolder) | Out-Null
            $staging = Join-Path $expansionFolder ('wotlk.partial-' + [Guid]::NewGuid().ToString('N'))
            [IO.Directory]::CreateDirectory($staging) | Out-Null
            try {
                Write-Host ('Copiando arquivos para ' + $staging)
                & robocopy.exe $sourceData $staging /E /R:1 /W:1 /XJ /NFL /NDL /NP
                $copyStatus = $LASTEXITCODE
                if ($copyStatus -gt 7) {
                    throw ('Falha na copia dos dados (robocopy codigo ' + $copyStatus + ').')
                }
                if (-not (Test-Path -LiteralPath $staging -PathType Container)) {
                    throw 'Pasta copiada nao encontrada.'
                }
                Move-Item -LiteralPath $staging -Destination $portableData -ErrorAction Stop
                $found = $portableRoot
                Write-Host 'Instalacao portatil criada com sucesso.'
            } catch {
                Write-Warning ('Copia interrompida. Arquivos parciais em: ' + $staging)
                throw
            }
        }
    }
    if (-not [WolIni]::WritePrivateProfileString('WorldOfLegends', 'DataPath', $found, $ini)) {
        throw 'Nao foi possivel gravar DataPath no arquivo INI.'
    }
    Write-Host "Dados do jogo: $found"
    Write-Host 'Configuracao concluida. O launcher sera aberto.'
    exit 0
} catch {
    [System.Windows.Forms.MessageBox]::Show(
        ('Falha ao configurar dados: ' + $_.Exception.Message),
        'World of Legends', 'OK', 'Error') | Out-Null
    exit 1
}
