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
            "Nao localizamos automaticamente expansions\wotlk. Selecione a pasta principal de uma instalacao legitima que contenha expansions\wotlk. Os arquivos nao serao copiados.",
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
