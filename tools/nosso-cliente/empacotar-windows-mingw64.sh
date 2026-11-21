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

dest="$root/dist/World-of-Legends-Windows"
mkdir -p "$root/dist"
rm -rf "$dest"
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
# Windres embeds an actual Windows icon resource, not merely an image beside the exe.
icon_win="$(cygpath -m "$icon")"
rc_file="$root/build/nosso-cliente-mingw64/world-of-legends-launcher.rc"
res_file="$root/build/nosso-cliente-mingw64/world-of-legends-launcher-res.o"
printf '1 ICON "%s"\\n' "$icon_win" > "$rc_file"
windres -i "$rc_file" -o "$res_file" -O coff
cp "$icon" "$dest/assets/world-of-legends.ico"

echo "Compilando iniciador grafico World of Legends.exe ..."
g++ -std=c++20 -O2 -municode -mwindows -static-libgcc -static-libstdc++ \
    "$root/tools/nosso-cliente/world-of-legends-launcher.cpp" "$res_file" \
    -o "$dest/World of Legends.exe" -luser32

# ldd includes transitive dependencies. Copy only MSYS2/MinGW DLLs, not Windows System32.
echo "Incluindo DLLs do ambiente MinGW64 ..."
while IFS= read -r dll; do
  [[ -f "$dll" ]] || continue
  cp -n "$dll" "$dest/"
done < <(ldd "$exe" | awk '{for (i=1;i<=NF;i++) if (index($i, "/mingw64/bin/") == 1 && $i ~ /[.]dll$/) print $i}' | sort -u)

if [[ -f "$bin/vulkan-1.dll" ]]; then
  cp "$bin/vulkan-1.dll" "$dest/"
fi

cat > "$dest/World-of-Legends.ini" <<'INI'
[WorldOfLegends]
; Pasta que contem expansions/wotlk (os dados originais ficam fora deste ZIP)
DataPath=E:\
INI

cat > "$dest/LEIA-ME.txt" <<'TXT'
WORLD OF LEGENDS - CLIENTE WINDOWS
1. Extraia a pasta completa, sem separar os arquivos.
2. Confira o arquivo World-of-Legends.ini:
   DataPath=E:\
   Esse caminho deve conter a subpasta expansions\wotlk extraida do seu jogo.
3. Inicie World of Legends.exe com dois cliques.
4. Mantenha MySQL, authserver e worldserver funcionando no PC que hospeda o reino.
5. Para diagnosticar problemas, veja logs na pasta de trabalho do cliente.
6. Nao distribua arquivos de dados proprietarios do jogo.
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

echo "CONCLUIDO: $dest"
echo "INICIADOR: $dest/World of Legends.exe"
echo "ZIP: $root/dist/World-of-Legends-Windows.zip"
