#!/usr/bin/env bash
# Creates a self-contained Windows client directory from the existing MinGW64 build.
# Run from MSYS2 MINGW64: bash tools/nosso-cliente/empacotar-windows-mingw64.sh
set -euo pipefail

if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
  echo "ERRO: abra MSYS2 MINGW64." >&2
  exit 1
fi
for cmd in g++ windres ldd cp find; do
  command -v "$cmd" >/dev/null || { echo "ERRO: faltando $cmd" >&2; exit 1; }
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root"
bin="$root/build/nosso-cliente-mingw64/bin"
exe="$bin/wowee.exe"
[[ -f "$exe" ]] || { echo "ERRO: compile antes o wowee.exe" >&2; exit 1; }
[[ -d "$root/assets" && -d "$root/Data" ]] || { echo "ERRO: assets/ ou Data/ ausente" >&2; exit 1; }
[[ -f "$root/assets/world-of-legends-login.png" && -f "$root/assets/world-of-legends-loading.jpg" ]] || {
  echo "ERRO: faltam as duas artes originais em assets/" >&2; exit 1;
}

# Keep the version displayed in the launcher aligned with the release archive.
version="$(tr -d '\r\n' < "$root/tools/nosso-cliente/world-of-legends-version.txt")"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "ERRO: versao invalida: $version" >&2
  exit 1
fi
printf '#pragma once\n#define WOL_LAUNCHER_VERSION L"%s"\n' "$version" > "$root/build/nosso-cliente-mingw64/world-of-legends-version.hpp"

dest="$root/dist/World-of-Legends-Windows"
backup="$root/dist/World-of-Legends-Windows-anterior"
mkdir -p "$root/dist"
# Keep the last known-working version in case this package fails validation.
if [[ -d "$dest" ]]; then
  rm -rf "$backup"
  mv "$dest" "$backup"
fi
mkdir -p "$dest"
cp "$exe" "$dest/wowee.exe"
cp -R "$bin/assets" "$dest/assets"
# Replace any stale copied art with the originals from the source assets folder.
cp "$root/assets/world-of-legends-login.png" "$dest/assets/"
cp "$root/assets/world-of-legends-loading.jpg" "$dest/assets/"
cp -R "$root/Data" "$dest/Data"

# The official icon is a binary file supplied separately; do not embed it as text in Git.
icon="$root/assets/world-of-legends.ico"
if [[ ! -f "$icon" ]]; then
  echo "ERRO: icone ausente: $icon" >&2
  echo "Extraia o pacote de icone para a pasta assets/ antes de empacotar." >&2
  exit 1
fi
# SDL_SetWindowIcon reads PNG at runtime; the embedded ICO alone only changes
# the file/shortcut icon. Both files must be shipped for the taskbar to match.
runtime_icon="$root/assets/world-of-legends.png"
if [[ ! -f "$runtime_icon" ]]; then
  echo "ERRO: icone PNG ausente: $runtime_icon" >&2
  echo "Coloque world-of-legends.png junto do .ico dentro de assets/." >&2
  exit 1
fi
cp "$runtime_icon" "$dest/assets/world-of-legends.png"
# Windres embeds an actual Windows icon resource, not merely an image beside the exe.
icon_win="$(cygpath -m "$icon")"
rc_file="$root/build/nosso-cliente-mingw64/world-of-legends-launcher.rc"
res_file="$root/build/nosso-cliente-mingw64/world-of-legends-launcher-res.o"
printf '1 ICON "%s"\n' "$icon_win" > "$rc_file"
windres -i "$rc_file" -o "$res_file" -O coff
cp "$icon" "$dest/assets/world-of-legends.ico"
printf 'v%s\n' "$version" > "$dest/World-of-Legends.version"

echo "Compilando iniciador grafico World of Legends.exe ..."
g++ -std=c++20 -O2 -municode -mwindows -static-libgcc -static-libstdc++ \
    -I"$root/build/nosso-cliente-mingw64" \
    "$root/tools/nosso-cliente/world-of-legends-launcher.cpp" "$res_file" \
    -o "$dest/World of Legends.exe" -luser32 -lgdi32 -lshell32 -lws2_32 -lgdiplus

# Include the local, inspected update checker. Downloaded content is never executed.
cp "$root/tools/nosso-cliente/world-of-legends-update.ps1" "$dest/world-of-legends-update.ps1"
cp "$root/tools/nosso-cliente/world-of-legends-install.ps1" "$dest/world-of-legends-install.ps1"

# ldd includes transitive dependencies. Copy only MSYS2/MinGW DLLs, not Windows System32.
echo "Incluindo DLLs do ambiente MinGW64 ..."
while IFS= read -r dll; do
  [[ -f "$dll" ]] || continue
  cp -n "$dll" "$dest/"
done < <(ldd "$exe" | awk '{for (i=1;i<=NF;i++) if (index($i, "/mingw64/bin/") == 1 && $i ~ /[.]dll$/) print $i}' | sort -u)

if [[ -f "$bin/vulkan-1.dll" ]]; then
  cp "$bin/vulkan-1.dll" "$dest/"
fi

# Preserve any custom data/host settings from the prior working package.
if [[ -f "$backup/World-of-Legends.ini" ]]; then
  cp "$backup/World-of-Legends.ini" "$dest/World-of-Legends.ini"
else
cat > "$dest/World-of-Legends.ini" <<'INI'
[WorldOfLegends]
; Pasta que contem expansions/wotlk (os dados originais ficam fora deste ZIP)
DataPath=E:\
; IP ou nome do computador que hospeda o reino (status TCP portas 3724/8085)
ServerHost=127.0.0.1
; Configuracoes reais aplicadas pelo launcher ao abrir o jogo
FileCacheMB=256
InitialTerrainRadius=2
PreviewMSAA=1
INI
fi

cat > "$dest/LEIA-ME.txt" <<'TXT'
WORLD OF LEGENDS - CLIENTE WINDOWS
1. Extraia a pasta completa, sem separar os arquivos.
2. Confira o arquivo World-of-Legends.ini:
   DataPath=E:\
   Esse caminho deve conter a subpasta expansions\wotlk extraida do seu jogo.
3. Inicie World of Legends.exe com dois cliques.
   Na janela, clique JOGAR; CONFIGURACOES abre uma janela para ajustar
   dados, servidor, cache, raio inicial do terreno e MSAA da previa 3D.
   As preferencias sao salvas em World-of-Legends.ini.
   ServerHost define o servidor verificado no status (portas 3724 e 8085).
   Resolucao e tela cheia devem ser ajustadas dentro do jogo.
4. VERIFICAR ATUALIZACOES consulta GitHub Releases. Releases precisam ter
   World-of-Legends-Windows.zip e World-of-Legends-Windows.zip.sha256.
   O pacote verificado fica em updates/; pode ser instalado apos confirmacao.
   A instalacao exige o jogo fechado e cria backup completo lado a lado.
   RESTAURAR VERSAO devolve a ultima instalacao preservada.
   Se ja houver backup de atualizacao, a nova instalacao e bloqueada
   ate voce arquivar ou restaurar o backup anterior.
5. Mantenha MySQL, authserver e worldserver funcionando no PC que hospeda o reino.
6. Para diagnosticar problemas, veja logs na pasta de trabalho do cliente.
7. Nao distribua arquivos de dados proprietarios do jogo.
TXT

# Verify no MinGW DLL was missed before making an archive.
missing="$(PATH="/usr/bin:/bin:$dest" ldd "$dest/wowee.exe" 2>&1 | grep -i 'not found' || true)"
if [[ -n "$missing" ]]; then
  echo "AVISO: dependencias possivelmente ausentes:" >&2
  echo "$missing" >&2
fi

echo "Criando ZIP..."
if command -v powershell.exe >/dev/null 2>&1; then
  export WOL_DIST_WIN="$(cygpath -w "$dest")"
  export WOL_ZIP_WIN="$(cygpath -w "$root/dist/World-of-Legends-Windows.zip")"
  powershell.exe -NoProfile -Command \
    'Compress-Archive -LiteralPath $env:WOL_DIST_WIN -DestinationPath $env:WOL_ZIP_WIN -Force'
elif command -v zip >/dev/null 2>&1; then
  (cd "$root/dist" && zip -qr World-of-Legends-Windows.zip World-of-Legends-Windows)
else
  echo "AVISO: sem criador de ZIP, a pasta esta pronta em $dest"
fi

# Matching checksum asset for publishing a verified GitHub Release.
zipfile="$root/dist/World-of-Legends-Windows.zip"
if [[ -f "$zipfile" ]] && command -v sha256sum >/dev/null 2>&1; then
  (cd "$root/dist" && sha256sum World-of-Legends-Windows.zip > World-of-Legends-Windows.zip.sha256)
  echo "SHA256: $root/dist/World-of-Legends-Windows.zip.sha256"
fi

echo "CONCLUIDO: $dest"
echo "INICIADOR: $dest/World of Legends.exe"
echo "ZIP: $root/dist/World-of-Legends-Windows.zip"
