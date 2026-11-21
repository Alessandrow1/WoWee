#!/usr/bin/env bash
# Cria um pacote separado para convidados, sem alterar a instalacao do dono.
# MSYS2 MINGW64:
# bash tools/nosso-cliente/preparar-convidado-windows.sh AUTH_HOST AUTH_PORT WORLD_HOST WORLD_PORT
set -euo pipefail

if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
  echo "ERRO: use o terminal MSYS2 MINGW64." >&2
  exit 1
fi
if [[ "$#" -ne 4 ]]; then
  echo "Uso: $0 AUTH_HOST AUTH_PORT WORLD_HOST WORLD_PORT" >&2
  exit 1
fi
auth_host="$1"; auth_port="$2"; world_host="$3"; world_port="$4"
for host in "$auth_host" "$world_host"; do
  if [[ ! "$host" =~ ^[a-zA-Z0-9.-]+$ ]]; then
    echo "ERRO: hostname invalido: $host" >&2
    exit 1
  fi
done
for port in "$auth_port" "$world_port"; do
  if [[ ! "$port" =~ ^[0-9]+$ ]] || (( 10#$port < 1 || 10#$port > 65535 )); then
    echo "ERRO: porta invalida: $port" >&2
    exit 1
  fi
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source_dir="$root/dist/World-of-Legends-Windows"
guest_dir="$root/dist/World-of-Legends-Convidado"
guest_zip="$root/dist/World-of-Legends-Convidado.zip"
[[ -f "$source_dir/World of Legends.exe" && -f "$source_dir/wowee.exe" ]] || {
  echo "ERRO: compile e empacote o launcher primeiro." >&2
  exit 1
}
# Evita empacotar uma instalacao com arquivos de dados comerciais do WoW.
# A pasta 'Data' de runtime do WoWee pode conter arquivos livres do projeto,
# mas MPQ/CASC proprietarios devem vir da instalacao legitima do jogador.
if find "$source_dir" -type f \( -iname '*.mpq' -o -iname '*.casc' \) -print -quit | grep -q .; then
  echo "ERRO: pacote contem arquivos de dados MPQ/CASC; remova-os antes de distribuir." >&2
  exit 1
fi
rm -rf "$guest_dir"
cp -R "$source_dir" "$guest_dir"
rm -rf "$guest_dir/updates"
rm -f "$guest_dir/wowee.exe.anterior"
# Configuracao personalizada e independente da instalacao do host.
cat > "$guest_dir/World-of-Legends.ini" <<INI
[WorldOfLegends]
; Ajuste para a pasta que contem expansions/wotlk no PC do jogador
DataPath=E:\\
ServerHost=$auth_host
ServerPort=$auth_port
WorldHost=$world_host
WorldPort=$world_port
FileCacheMB=256
InitialTerrainRadius=2
PreviewMSAA=1
INI
cat > "$guest_dir/COMECE-AQUI.txt" <<'TXT'
WORLD OF LEGENDS - CONVIDADO
1. Extraia a pasta completa no seu computador.
2. Abra World of Legends.exe.
3. Em CONFIGURACOES, ajuste CAMINHO DOS DADOS para a pasta
   onde voce possui legitimamente expansions/wotlk.
4. O endereco do reino ja vem configurado. Nao precisa de VPN.
5. Peca ao administrador uma conta individual do World of Legends.
6. O servidor do anfitriao precisa ficar ligado para jogar.
7. O pacote nao inclui os dados proprietarios do jogo.
TXT
if command -v powershell.exe >/dev/null; then
  export WOL_GUEST_DIR="$(cygpath -w "$guest_dir")"
  export WOL_GUEST_ZIP="$(cygpath -w "$guest_zip")"
  powershell.exe -NoProfile -Command 'Compress-Archive -LiteralPath $env:WOL_GUEST_DIR -DestinationPath $env:WOL_GUEST_ZIP -Force'
elif command -v zip >/dev/null; then
  (cd "$root/dist" && zip -qr "World-of-Legends-Convidado.zip" "World-of-Legends-Convidado")
else
  echo "ERRO: powershell.exe ou zip necessario para criar o pacote." >&2
  exit 1
fi
echo "PACOTE DE CONVIDADO: $guest_zip"
