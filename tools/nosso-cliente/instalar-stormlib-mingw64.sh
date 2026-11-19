#!/usr/bin/env bash
# Instala StormLib estaticamente no prefixo MSYS2 MINGW64 para habilitar
# o gerenciador/extrator de arquivos MPQ do WoWee.
# Execute conscientemente no terminal MSYS2 MINGW64 (na instalacao desejada).
set -euo pipefail

if [[ "${MSYSTEM:-}" != "MINGW64" || "${MINGW_PREFIX:-}" != "/mingw64" ]]; then
  echo "ERRO: execute no terminal MSYS2 MINGW64 (nao UCRT64, MSYS ou PowerShell)." >&2
  exit 1
fi
for c in pacman git cmake ninja g++; do
  if ! command -v "$c" >/dev/null 2>&1; then
    echo "ERRO: comando obrigatorio ausente: $c" >&2
    exit 1
  fi
done

echo "Prefixo da instalacao: $(cygpath -w "$MINGW_PREFIX")"
echo "Instalando LibTomCrypt e LibTomMath necessarias ao StormLib estatico..."
pacman -S --needed mingw-w64-x86_64-libtommath mingw-w64-x86_64-libtomcrypt

source_dir="$HOME/.cache/wowee-stormlib-src"
build_dir="$HOME/.cache/wowee-stormlib-build"
mkdir -p "$HOME/.cache"

if [[ ! -e "$source_dir" ]]; then
  git clone --depth 1 https://github.com/ladislav-zezula/StormLib.git "$source_dir"
elif [[ ! -d "$source_dir/.git" ]]; then
  echo "ERRO: $source_dir existe mas nao e um clone Git." >&2
  exit 1
else
  echo "Reutilizando fonte existente em $source_dir (sem atualizar automaticamente)."
fi

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
  -DBUILD_SHARED_LIBS=OFF

cmake --build "$build_dir" --parallel 4
cmake --install "$build_dir"

echo "StormLib instalado no MINGW64."
echo "Agora execute no repo WoWee: bash tools/nosso-cliente/compilar-windows-mingw64.sh"
