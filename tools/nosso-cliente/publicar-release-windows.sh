#!/usr/bin/env bash
# Publish a tested Windows portable package to GitHub Releases.
# Usage: bash tools/nosso-cliente/publicar-release-windows.sh v0.3.0
set -euo pipefail
tag="$1"
if [[ ! "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "ERRO: use tag como v0.3.0"; exit 1
fi
if [[ "$MSYSTEM" != "MINGW64" ]]; then
  echo "ERRO: execute no MSYS2 MINGW64."; exit 1
fi
# The GitHub CLI Windows MSI normally installs here; MSYS2 may not inherit
# the updated Windows PATH even after the CLI has been installed.
if ! command -v gh >/dev/null 2>&1; then
  for github_cli_dir in "/c/Program Files/GitHub CLI" "/c/Program Files (x86)/GitHub CLI"; do
    if [[ -f "$github_cli_dir/gh.exe" ]]; then
      export PATH="$PATH:$github_cli_dir"
      break
    fi
  done
fi
for app in gh sha256sum unzip git; do
  command -v "$app" >/dev/null || { echo "ERRO: falta $app"; exit 1; }
done
cd "$(dirname "$0")/../.."
repo="Alessandrow1/WoWee"
zipfile="dist/World-of-Legends-Windows.zip"
hashfile="dist/World-of-Legends-Windows.zip.sha256"
test -s "$zipfile" && test -s "$hashfile" || {
  echo "ERRO: execute o empacotador para gerar ZIP e SHA256."; exit 1;
}
(cd dist && sha256sum -c World-of-Legends-Windows.zip.sha256)
contents="$(unzip -Z -1 "$zipfile" | tr -d '\r')"
for required in \
  "World-of-Legends-Windows/World of Legends.exe" \
  "World-of-Legends-Windows/wowee.exe" \
  "World-of-Legends-Windows/world-of-legends-update.ps1" \
  "World-of-Legends-Windows/World-of-Legends.ini" \
  "World-of-Legends-Windows/assets/world-of-legends.ico" \
  "World-of-Legends-Windows/assets/world-of-legends.png"; do
  if ! printf '%s\n' "$contents" | grep -Fxq "$required"; then
    echo "ERRO: arquivo ausente dentro do ZIP: $required"; exit 1
  fi
done
# The WoWee build regenerates tracked SPIR-V shaders and opcode registries.
# Those generated files are expected to differ after compilation. Do not
# reset/delete them, and do not block a tested binary release because of them.
# Keep blocking actual changes to source code, scripts and configuration.
unexpected=()
while IFS= read -r modified; do
  [[ -n "$modified" ]] || continue
  case "$modified" in
    assets/shaders/*.spv|include/game/opcode_*_generated.inc)
      ;;
    extern/vk-bootstrap)
      echo "AVISO: submodulo vk-bootstrap aparece modificado localmente."
      echo "Confirme que o executavel compilado foi testado antes de publicar."
      ;;
    *)
      unexpected+=("$modified")
      ;;
  esac
done < <(git diff --name-only HEAD --)
if [[ ${#unexpected[@]} -gt 0 ]]; then
  echo "ERRO: arquivos de codigo/configuracao modificados sem commit:"
  printf '  %s\n' "${unexpected[@]}"
  echo "Revise e registre as alteracoes antes da release."
  exit 1
fi
echo "OK: alteracoes locais geradas pelo build nao impedem a release."

if [[ "$(git branch --show-current)" != "develop" ]]; then
  echo "ERRO: execute na branch develop."; exit 1
fi
gh auth status || { echo "ERRO: login necessario (gh auth login)."; exit 1; }
if gh release view "$tag" --repo "$repo" >/dev/null 2>&1; then
  echo "ERRO: release ja existe. Nao vou sobrescrever."; exit 1
fi
echo "Release: $tag | commit $(git rev-parse --short HEAD)"
echo "ZIP e SHA256 validados. Confirme somente apos testar launcher e jogo."
read -r -p "Digite PUBLICAR para criar a release publica: " answer
if [[ "${answer^^}" != "PUBLICAR" ]]; then echo "Cancelado."; exit 0; fi
gh release create "$tag" "$zipfile" "$hashfile" \
  --repo "$repo" \
  --target "$(git rev-parse HEAD)" \
  --title "World of Legends $tag - Windows" \
  --notes "Cliente portatil Windows com launcher, configuracoes e atualizador de verificacao SHA-256. Requer dados originais do jogo instalados separadamente. O atualizador apenas baixa pacotes verificados; instalacao automatica ainda nao esta habilitada." \
  --latest
echo "Publicada: https://github.com/$repo/releases/tag/$tag"
