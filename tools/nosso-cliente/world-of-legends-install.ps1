param(
 [ValidateSet('Install','Rollback')][string]$Mode = 'Install',
 [Parameter(Mandatory=$true)][string]$InstallDir,
 [string]$ArchivePath,
 [string]$ChecksumPath,
 [string]$ExpectedVersion,
 [int]$LauncherPid = 0,
 [int]$UpdaterPid = 0
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Alert([string]$text,[bool]$failure=$false) {
 Write-Host $text
 $kind=if($failure){'Error'}else{'Information'}
 [System.Windows.Forms.MessageBox]::Show($text,'World of Legends','OK',$kind)|Out-Null
}
function Closed {
 if(Get-Process -Name 'wowee' -ErrorAction SilentlyContinue) {
   throw 'Feche o jogo antes de instalar ou restaurar.'
 }
 if($LauncherPid -gt 0) {
   $launcher=Get-Process -Id $LauncherPid -ErrorAction SilentlyContinue
   if($launcher) {
     [void]$launcher.CloseMainWindow()
     $launcher.WaitForExit(45000)|Out-Null
     if(-not $launcher.HasExited){throw 'Feche o launcher e tente novamente.'}
   }
 }
 if(Get-Process -Name 'wowee' -ErrorAction SilentlyContinue) {
   throw 'O jogo ainda esta aberto.'
 }
}
function Verified([string]$zip,[string]$sha) {
 if(-not (Test-Path -LiteralPath $zip -PathType Leaf)){throw 'ZIP ausente.'}
 if(-not (Test-Path -LiteralPath $sha -PathType Leaf)){throw 'Arquivo SHA256 ausente.'}
 $line=(Get-Content -LiteralPath $sha -Raw).Trim()
 if($line -notmatch '(?i)^([a-f0-9]{64})\s+\*?World-of-Legends-Windows\.zip$') {
   throw 'Formato de checksum invalido.'
 }
 $expected=$Matches[1].ToUpperInvariant()
 if((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToUpperInvariant() -ne $expected) {
   throw 'SHA256 incorreto. A instalacao foi bloqueada.'
 }
}
function ExtractSafe([string]$zip,[string]$targetRoot) {
 $archive=[IO.Compression.ZipFile]::OpenRead($zip)
 try {
   $entries=@($archive.Entries)
   if($entries.Count -eq 0 -or $entries.Count -gt 100000){throw 'ZIP invalido.'}
   $seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
   $total=[long]0
   foreach($e in $entries) {
     $name=$e.FullName.Replace('\','/')
     if(-not $name.StartsWith('World-of-Legends-Windows/',[StringComparison]::Ordinal) -or
        $name.Contains(':') -or
        $name.Split('/') -contains '..' -or
        $name.Split('/') -contains '.' -or
        (($e.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000) {
        throw ('Caminho ZIP inseguro: '+$name)
     }
     if(-not $seen.Add($name.TrimEnd('/'))){throw ('Arquivo ZIP duplicado: '+$name)}
     $total += [long]$e.Length
     if($total -gt 4GB){throw 'ZIP descompactado muito grande.'}
   }
   $i=0
   foreach($e in $entries) {
     $relative=$e.FullName.Replace('\','/').Substring('World-of-Legends-Windows/'.Length)
     if($relative.Length -eq 0){continue}
     $output=Join-Path $targetRoot ($relative.Replace('/',[IO.Path]::DirectorySeparatorChar))
     if($e.FullName.EndsWith('/')) {
       [IO.Directory]::CreateDirectory($output)|Out-Null
     } else {
       [IO.Directory]::CreateDirectory((Split-Path -Parent $output))|Out-Null
       [IO.Compression.ZipFileExtensions]::ExtractToFile($e,$output,$false)
     }
     $i++
     if(($i % 40) -eq 0 -or $i -eq $entries.Count) {
       Write-Progress -Activity 'Preparando World of Legends' -Status ($i.ToString()+' arquivos') -PercentComplete ([int](100*$i/$entries.Count))
     }
   }
   Write-Progress -Activity 'Preparando World of Legends' -Completed
 } finally { $archive.Dispose() }
}
try {
 $live=[IO.Path]::GetFullPath($InstallDir).TrimEnd('\')
 if(-not (Test-Path -LiteralPath $live -PathType Container)){throw 'Instalacao nao encontrada.'}
 foreach($f in @('World of Legends.exe','wowee.exe')) {
   if(-not (Test-Path -LiteralPath (Join-Path $live $f) -PathType Leaf)){throw ('Instalacao invalida: '+$f)}
 }
 $parent=Split-Path -Parent $live
 $leaf=Split-Path -Leaf $live
 $backup=Join-Path $parent ($leaf+'.backup')
 $temp=Join-Path $parent ($leaf+'.stage-'+[Guid]::NewGuid().ToString('N'))
 if($UpdaterPid -gt 0) {
   $updater=Get-Process -Id $UpdaterPid -ErrorAction SilentlyContinue
   if($updater) {
     $updater.WaitForExit(45000)|Out-Null
     if(-not $updater.HasExited){throw 'Processo do verificador ainda esta aberto.'}
   }
 }
 if($Mode -eq 'Rollback') {
   Closed
   if(-not (Test-Path -LiteralPath $backup -PathType Container)){throw 'Backup nao encontrado.'}
   $replaced=Join-Path $parent ($leaf+'.replaced-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
   if(Test-Path -LiteralPath $replaced){throw 'Pasta de restauracao ja existe.'}
   Rename-Item -LiteralPath $live -NewName (Split-Path -Leaf $replaced)
   try { Rename-Item -LiteralPath $backup -NewName $leaf }
   catch {
     Rename-Item -LiteralPath $replaced -NewName $leaf
     throw
   }
   Alert ('Versao anterior restaurada. A instalacao substituida foi preservada em: '+$replaced)
   exit 0
 }
 if($ExpectedVersion -notmatch '^v\d+\.\d+\.\d+$'){throw 'Versao solicitada invalida.'}
 Verified $ArchivePath $ChecksumPath
 if(Test-Path -LiteralPath $backup){
   throw ('Backup anterior ja existe: '+$backup+'. Restaure ou arquive antes de continuar.')
 }
 [IO.Directory]::CreateDirectory($temp)|Out-Null
 try {
   ExtractSafe $ArchivePath $temp
   foreach($f in @('World of Legends.exe','wowee.exe','world-of-legends-update.ps1',
        'world-of-legends-install.ps1','World-of-Legends.version',
        'assets/world-of-legends.ico','assets/world-of-legends.png')) {
     if(-not (Test-Path -LiteralPath (Join-Path $temp $f) -PathType Leaf)){
       throw ('Arquivo obrigatorio ausente: '+$f)
     }
   }
   $actualVersion=(Get-Content -LiteralPath (Join-Path $temp 'World-of-Legends.version') -Raw).Trim()
   if($actualVersion -ne $ExpectedVersion){throw ('Versao diferente da solicitada: '+$actualVersion)}
   $ini=Join-Path $live 'World-of-Legends.ini'
   if(Test-Path -LiteralPath $ini){Copy-Item -LiteralPath $ini -Destination (Join-Path $temp 'World-of-Legends.ini') -Force}
   foreach($folder in @('logs','updates')) {
     $src=Join-Path $live $folder
     if(Test-Path -LiteralPath $src -PathType Container){
       Copy-Item -LiteralPath $src -Destination (Join-Path $temp $folder) -Recurse -Force
     }
   }
   Closed
   Rename-Item -LiteralPath $live -NewName (Split-Path -Leaf $backup)
   try { Rename-Item -LiteralPath $temp -NewName $leaf }
   catch {
     Rename-Item -LiteralPath $backup -NewName $leaf
     throw
   }
 } finally {
   if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue}
 }
 Alert ('Atualizado para '+$ExpectedVersion+'. Backup preservado em '+$backup+'. Abra novamente o launcher.')
} catch {
 Alert ('Falha: '+$_.Exception.Message+'. A instalacao anterior deve ser mantida ou recuperada do backup.') $true
 exit 1
}
