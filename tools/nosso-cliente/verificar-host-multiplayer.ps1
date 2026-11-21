# World of Legends - diagnostico de acesso remoto (somente leitura).
# Execute no Windows PowerShell do computador que hospeda o AzerothCore.
# Nao altera banco, roteador nem firewall.
[CmdletBinding()]
param(
    [string]$ServerDir = 'E:\World-of-Legends\Build\bin\Release'
)
$ErrorActionPreference = 'Continue'
Write-Host ''
Write-Host '=== WORLD OF LEGENDS - PREPARACAO MULTIPLAYER ===' -ForegroundColor Cyan
Write-Host ('Servidor esperado: ' + $ServerDir)
Write-Host ''

foreach ($serviceName in @('MySQL84')) {
    $service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
    if ($null -eq $service) {
        Write-Host ("Servico {0}: NAO ENCONTRADO (verifique o nome do MySQL)" -f $serviceName) -ForegroundColor Yellow
    } else {
        Write-Host ("Servico {0}: {1}" -f $serviceName,$service.Status)
    }
}
foreach ($name in @('authserver','worldserver')) {
    $process = Get-Process -Name $name -ErrorAction SilentlyContinue
    Write-Host ("Processo {0}: {1}" -f $name, $(if ($process) { 'EXECUTANDO' } else { 'PARADO' }))
}
Write-Host ''
foreach ($port in @(3724,8085)) {
    $tcp = @(Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue)
    if ($tcp.Count -eq 0) {
        Write-Host ("TCP {0}: NAO ESTA ESCUTANDO" -f $port) -ForegroundColor Yellow
        continue
    }
    foreach ($entry in $tcp) {
        $addr = $entry.LocalAddress
        $scope = if ($addr -eq '127.0.0.1' -or $addr -eq '::1') { 'SOMENTE LOCAL' } else { 'ACEITA CONEXOES NA INTERFACE' }
        Write-Host ("TCP {0}: {1} ({2}) PID={3}" -f $port,$addr,$scope,$entry.OwningProcess)
    }
}
Write-Host ''
foreach ($port in @(3724,8085)) {
    $rules = @(Get-NetFirewallPortFilter -ErrorAction SilentlyContinue | Where-Object {
        $_.Protocol -eq 'TCP' -and $_.LocalPort -eq [string]$port
    })
    if ($rules.Count -eq 0) {
        Write-Host ("Firewall TCP {0}: nenhuma regra de porta especifica encontrada" -f $port) -ForegroundColor Yellow
    } else {
        foreach ($filter in $rules) {
            $rule = $filter | Get-NetFirewallRule -ErrorAction SilentlyContinue
            if ($rule) {
                Write-Host ("Firewall TCP {0}: {1} | Acao={2} | Ativa={3} | Perfil={4}" -f $port,$rule.DisplayName,$rule.Action,$rule.Enabled,$rule.Profile)
            }
        }
    }
}
Write-Host ''
if (Test-Path -LiteralPath $ServerDir) {
    foreach ($name in @('authserver.exe','worldserver.exe')) {
        Write-Host ("Arquivo {0}: {1}" -f $name,$(if (Test-Path (Join-Path $ServerDir $name)) {'OK'} else {'AUSENTE'}))
    }
} else {
    Write-Host 'Pasta do servidor nao encontrada. Confira -ServerDir.' -ForegroundColor Yellow
}
Write-Host ''
Write-Host 'PROXIMO: conferir o endereco do realmlist (acore_auth), o IP WAN do roteador e se existe CGNAT.'
Write-Host 'Este teste local NAO confirma acesso pela internet. Nao compartilhe senhas, tokens ou IP publico aqui.'
