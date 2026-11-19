# Instalação MSYS2 para nossa compilação WoWee — Windows x64

Este guia se aplica ao shell **MSYS2 MINGW64** (`MSYSTEM=MINGW64`).

## 1. Atualizar o MSYS2

```bash
pacman -Syu
```

Se solicitar fechamento do shell, abra o MSYS2 MINGW64 novamente e repita o comando.

## 2. Instalar a toolchain

O guia upstream lista `mingw-w64-x86_64-unicorn`, mas esse pacote **não está no repositório MINGW64 atual**. Para a primeira compilação, omita-o:

```bash
pacman -S --needed \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-pkgconf \
  mingw-w64-x86_64-sdl3 \
  mingw-w64-x86_64-glm \
  mingw-w64-x86_64-openssl \
  mingw-w64-x86_64-zlib \
  mingw-w64-x86_64-ffmpeg \
  mingw-w64-x86_64-vulkan-loader \
  mingw-w64-x86_64-vulkan-headers \
  mingw-w64-x86_64-shaderc \
  git
```

**Não instale** `mingw-w64-ucrt-x86_64-unicorn` dentro de MINGW64: esse pacote é para o ambiente UCRT64. O WoWee compila sem Unicorn; nesse caso, emulação Warden fica desabilitada, o que pode impedir conexão a alguns servidores.

## 3. Conferir ferramentas

```bash
echo "$MSYSTEM"
cmake --version
ninja --version
g++ --version
glslc --version
```

## 4. Compilar

```bash
cd /d/Projetos/WoWee
git pull --ff-only origin develop
bash tools/nosso-cliente/compilar-windows-mingw64.sh
```

O script compila sem executar, extrair ou distribuir dados proprietários do jogo; informe o log caso a configuração CMake ou a build falhe.

Fontes: [documentação do WoWee](https://github.com/Kelsidavis/WoWee/blob/master/BUILD_INSTRUCTIONS.md), [pacotes MSYS2 unicorn](https://packages.msys2.org/base/mingw-w64-unicorn).
