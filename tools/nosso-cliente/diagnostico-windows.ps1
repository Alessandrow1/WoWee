# Diagnostico somente leitura — Windows PowerShell 5.1+.
$ErrorActionPreference = 'Continue'
Write-Host '==== Windows / GPU / Ferramentas ====' -ForegroundColor Cyan
try { Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, OSArchitecture | Format-List } catch { Write-Warning $_ }
try { Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors | Format-List } catch { Write-Warning $_ }
try { Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, VideoProcessor | Format-List } catch { Write-Warning $_ }
foreach ($nome in @('git', 'gh', 'cmake', 'vulkaninfo', 'glslc')) {
    $cmd = Get-Command $nome -ErrorAction SilentlyContinue
    if ($cmd) { Write-Host "OK: $nome -> $($cmd.Source)" -ForegroundColor Green }
    else { Write-Host "AUSENTE NO PATH: $nome" -ForegroundColor Yellow }
}
if (Get-Command vulkaninfo -ErrorAction SilentlyContinue) {
    Write-Host "`n==== Vulkan ====" -ForegroundColor Cyan
    & vulkaninfo --summary
    if ($LASTEXITCODE -ne 0) { Write-Warning 'vulkaninfo falhou; verifique driver e SDK.' }
} else { Write-Host 'vulkaninfo nao esta instalado; Vulkan nao confirmado.' }
Write-Host "`nEsse diagnostico NAO mede FPS nem certifica compatibilidade."
