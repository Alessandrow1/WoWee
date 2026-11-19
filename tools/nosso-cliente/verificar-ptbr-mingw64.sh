#!/usr/bin/env bash
# Diagnostico somente leitura: confirma se o wowee.exe desta build
# carrega a identidade temporaria WoWee PT-BR.
set -euo pipefail

if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
  echo "AVISO: recomendado executar no MSYS2 MINGW64."
fi

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root"
exe="build/nosso-cliente-mingw64/bin/wowee.exe"
source="src/ui/auth_screen.cpp"
marker="WoWee PT-BR"

echo "== Repositorio: $root =="
echo "Branch local: $(git branch --show-current)"
echo "Commit local: $(git log -1 --format='%h %s')"
echo "Estado Git:"
git status --short

echo
if grep -Fq "\"$marker\"" "$source"; then
  echo "OK: codigo-fonte local contem o marcador '$marker'."
else
  echo "FALTA: codigo-fonte local ainda nao contem '$marker'."
  echo "Verifique 'git pull --ff-only origin develop' (sem descartar alteracoes locais)."
  exit 2
fi

echo
if [[ ! -f "$exe" ]]; then
  echo "FALTA: executavel nao encontrado: $root/$exe"
  echo "Compile com: bash tools/nosso-cliente/compilar-windows-mingw64.sh"
  exit 3
fi
echo "Executavel testado: $root/$exe"
ls -lh "$exe"

# A cadeia ASCII do titulo e usada pela tela de login; pode ser verificada
# diretamente nos bytes do PE produzido por MinGW, sem executar o jogo.
if LC_ALL=C grep -aFq "$marker" "$exe"; then
  echo "OK: executavel contem o marcador '$marker'."
  echo "Abra ESTE arquivo pelo MSYS2 MINGW64: ./$exe"
else
  echo "FALTA: executavel nao contem o marcador '$marker'."
  echo "Recompile depois de atualizar o codigo com:"
  echo "  bash tools/nosso-cliente/compilar-windows-mingw64.sh"
  exit 4
fi

echo
echo "Obs.: a traducao inicial cobre login e painel grafico; outras telas ainda podem estar em ingles."
