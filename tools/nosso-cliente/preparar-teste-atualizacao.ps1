# Prepares an isolated v0.3.2 client for testing the v0.3.3 updater.
# Must run AFTER generating the v0.3.3 package. Does not change the live client.
param(
 [string]$RepositoryRoot = (Join-Path $PSScriptRoot '..\..')
)
$ErrorActionPreference='Stop'
try {
 $root=[IO.Path]::GetFullPath($RepositoryRoot)
 $dist=Join-Path $root 'dist'
 $source=Join-Path $dist 'World-of-Legends-Windows-anterior'
 $test=Join-Path $dist 'World-of-Legends-Teste-Atualizacao'
 $latest=Join-Path $dist 'World-of-Legends-Windows'
 if(-not (Test-Path -LiteralPath $source -PathType Container)){
   throw 'Pacote anterior ausente. Primeiro gere a v0.3.3 usando o empacotador.'
 }
 if(Test-Path -LiteralPath $test) {
   throw ('A pasta de teste ja existe: '+$test+'. Nao vou sobrescreve-la.')
 }
 $oldVersion=Join-Path $source 'World-of-Legends.version'
 $newVersion=Join-Path $latest 'World-of-Legends.version'
 if(-not (Test-Path -LiteralPath $oldVersion) -or
    (Get-Content -LiteralPath $oldVersion -Raw).Trim() -ne 'v0.3.2'){
   throw 'O pacote anterior nao e v0.3.2. Nao vou usar uma origem diferente.'
 }
 if(-not (Test-Path -LiteralPath $newVersion) -or
    (Get-Content -LiteralPath $newVersion -Raw).Trim() -ne 'v0.3.3'){
   throw 'O novo pacote ainda nao e v0.3.3.'
 }
 foreach($name in @('World of Legends.exe','wowee.exe',
          'world-of-legends-update.ps1','world-of-legends-install.ps1')){
   if(-not (Test-Path -LiteralPath (Join-Path $source $name) -PathType Leaf)){
     throw ('Arquivo ausente no pacote anterior: '+$name)
   }
 }
 Write-Host 'Copiando a instalacao antiga para uma pasta de TESTE isolada...'
 Copy-Item -LiteralPath $source -Destination $test -Recurse -ErrorAction Stop
 $testVersion=(Get-Content -LiteralPath (Join-Path $test 'World-of-Legends.version') -Raw).Trim()
 if($testVersion -ne 'v0.3.2'){throw 'Copia de teste invalida.'}
 Write-Host ''
 Write-Host 'PRONTO: teste isolado preparado.'
 Write-Host ('Pasta: '+$test)
 Write-Host ('Launcher: '+(Join-Path $test 'World of Legends.exe'))
 Write-Host 'Depois da publicacao da v0.3.3, abra APENAS este launcher e teste a atualizacao.'
 Write-Host 'O instalador criara World-of-Legends-Teste-Atualizacao.backup ao lado desta pasta.'
} catch {
 Write-Host ('ERRO: '+$_.Exception.Message) -ForegroundColor Red
 exit 1
}
