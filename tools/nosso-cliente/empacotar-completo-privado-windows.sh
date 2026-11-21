#!/usr/bin/env bash
# Empacotamento COMPLETO e PRIVADO: use somente se tiver permissao expressa
# para compartilhar os dados WotLK com o destinatario especifico.
# O script nao faz upload dos arquivos para o GitHub.
# Uso: bash tools/nosso-cliente/empacotar-completo-privado-windows.sh
set -euo pipefail

if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
  echo "ERRO: abra o MSYS2 MINGW64." >&2
  exit 1
fi

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
data_root="${WOL_WOTLK_DATA_ROOT:-/e/expansions/wotlk}"
source_dir="$root/dist/World-of-Legends-Convidado"
dest_dir="$root/dist/World-of-Legends-Completo-Privado"
archive="$root/dist/World-of-Legends-Completo-Privado.zip"
data_dest="$dest_dir/expansions/wotlk"

[[ -f "$source_dir/World of Legends.exe" && -f "$source_dir/wowee.exe" &&
   -f "$source_dir/INICIAR-WOL.cmd" ]] || {
  echo "ERRO: gere primeiro o ZIP de convidado com preparar-convidado-windows.sh." >&2
  exit 1
}
[[ -d "$data_root" && -f "$data_root/expansion.json" ]] || {
  echo "ERRO: dados WotLK nao encontrados em: $data_root" >&2
  echo "Se estiverem em outra pasta, defina WOL_WOTLK_DATA_ROOT antes de executar." >&2
  exit 1
}
if find "$data_root" -type l -print -quit | grep -q .; then
  echo "ERRO: a origem contem links simbolicos; nao copiaremos arquivos externos a pasta." >&2
  exit 1
fi
if [[ -d "$source_dir/expansions" ]]; then
  echo "ERRO: pacote de convidado ja contem expansions. Reveja os arquivos antes de continuar." >&2
  exit 1
fi

seven_zip=""
for cmd in 7z 7zz 7za; do
  if command -v "$cmd" >/dev/null 2>&1; then
    seven_zip="$(command -v "$cmd")"
    break
  fi
done
if [[ -z "$seven_zip" ]]; then
  for installed in "/c/Program Files/7-Zip/7z.exe" "/c/Program Files (x86)/7-Zip/7z.exe"; do
    if [[ -f "$installed" ]]; then
      seven_zip="$installed"
      break
    fi
  done
fi
if [[ -z "$seven_zip" ]]; then
  echo "ERRO: 7-Zip nao encontrado. Instale o 7-Zip para criar ZIP64 com arquivos grandes." >&2
  exit 1
fi

echo "AVISO: a pasta de dados pode ter mais de 18 GiB."
echo "AVISO: esta operacao consome espaco para uma copia dos dados E para o ZIP."
echo "AVISO: NAO publique o ZIP completo em um GitHub Release ou repositorio."
echo "Origem dos dados: $data_root"
echo "Pacote privado: $archive"

# Cria uma pasta dedicada, sem tocar no pacote original de convidados.
rm -rf "$dest_dir"
cp -a "$source_dir" "$dest_dir"
rm -rf "$dest_dir/logs" "$dest_dir/updates"
mkdir -p "$dest_dir/expansions"
echo "Copiando dados para o pacote privado (pode ser demorado)..."
cp -a "$data_root" "$data_dest"

# Evita levar preferencias de contas, credenciais ou endpoints antigos do WoW.
rm -rf "$data_dest/wtf" "$data_dest/WTF" "$data_dest/cache" "$data_dest/Cache" \
       "$data_dest/logs" "$data_dest/Logs" "$data_dest/screenshots" "$data_dest/Screenshots"
rm -f "$data_dest/realmlist.wtf" "$data_dest/wow.ini"

cat > "$dest_dir/LEIA-ME-PRIVADO.txt" <<'TXT'
WORLD OF LEGENDS - PACOTE COMPLETO PARA USO PRIVADO AUTORIZADO
1. Extraia o ZIP inteiro em uma pasta do computador.
2. Execute INICIAR-WOL.cmd para ajustar o caminho dos dados locais.
3. O configurador encontra expansions/wotlk na propria pasta do WoL.
4. Os endpoints AUTH e WORLD ja estao no World-of-Legends.ini.
5. Utilize uma conta individual criada pelo administrador do servidor.
6. O servidor do anfitriao precisa estar ligado para jogar.
7. Nao publique nem redistribua este pacote alem do alcance de sua permissao.
TXT

rm -f "$archive" "$archive.sha256"
echo "Criando ZIP64 com 7-Zip..."
(
  cd "$root/dist"
  "$seven_zip" a -tzip -mx=1 -mmt=on "World-of-Legends-Completo-Privado.zip" "World-of-Legends-Completo-Privado" -y
)
echo "Verificando integridade do ZIP..."
"$seven_zip" t "$archive"
if command -v sha256sum >/dev/null 2>&1; then
  (cd "$root/dist" && sha256sum "World-of-Legends-Completo-Privado.zip" > "World-of-Legends-Completo-Privado.zip.sha256")
fi
echo "CONCLUIDO: $archive"
echo "ATENCAO: arquivo privado; envie apenas ao destinatario autorizado."
