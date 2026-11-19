#!/usr/bin/env bash
# Compila a versao de referencia do WoWee em Windows via MSYS2 MINGW64.
# Uso: ./tools/nosso-cliente/compilar-windows-mingw64.sh
set -euo pipefail

if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
  echo "ERRO: Abra o terminal MSYS2 MINGW64 (MSYSTEM=MINGW64)." >&2
  exit 1
fi

for cmd in cmake ninja g++ glslc; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "ERRO: ferramenta ausente: $cmd" >&2
    echo "Instale as dependencias conforme BUILD_INSTRUCTIONS.md (MSYS2 MINGW64)." >&2
    exit 1
  fi
done

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "$script_dir/../.." && pwd)"
cd "$root"

if [[ ! -f CMakeLists.txt ]]; then
  echo "ERRO: CMakeLists.txt nao encontrado em $root" >&2
  exit 1
fi

if [[ ! -f extern/imgui/imgui.h || ! -f extern/vk-bootstrap/src/VkBootstrap.cpp ]]; then
  echo "AVISO: submodulos possivelmente incompletos. Confirme com git submodule status." >&2
fi

echo "Raiz: $root"
echo "Configurando CMake em build/nosso-cliente-mingw64 ..."
cmake -S . -B build/nosso-cliente-mingw64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWOWEE_BUILD_TESTS=OFF \
  -DWOWEE_ENABLE_LTO=OFF \
  -DWOWEE_ENABLE_AMD_FSR2=OFF \
  -DWOWEE_ENABLE_AMD_FSR3_FRAMEGEN=OFF

echo "Compilando o wowee.exe com no maximo 4 tarefas paralelas ..."
echo "Nota: mudar LTO de ON para OFF exige recompilar os objetos do executavel uma vez."
echo "Na etapa de link o Ninja pode ficar sem imprimir novas linhas; nao interrompa."
cmake --build build/nosso-cliente-mingw64 --target wowee --parallel 4

echo "Compilacao terminou. Localize o executavel:"
find build/nosso-cliente-mingw64 -type f \( -iname 'wowee.exe' -o -iname 'wowee_assets.exe' \) -print
echo "Este passo nao certifica performance, nem instala dados do jogo."
