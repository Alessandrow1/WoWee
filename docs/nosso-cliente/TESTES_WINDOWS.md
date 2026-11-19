# Testes iniciais — Windows 11 x64

## Ambiente observado em 19/11/2026
- Windows 11 Pro x64
- Ryzen 7 5700X3D (8 núcleos / 16 threads)
- NVIDIA GeForce GT 1030, driver 582.66
- `vulkaninfo --summary`: Vulkan 1.4.312 na GPU
- Git e GitHub CLI presentes; CMake e `glslc` ausentes do PATH
- Aviso de layer do Epic Online Services no loader Vulkan: investigar somente se afetar a execução

## Baseline oficial
- [ ] Extrair a versão Windows x86-64 oficial, separada da pasta de desenvolvimento
- [ ] Verificar se abre janela e detecta o dispositivo Vulkan
- [ ] Verificar acesso ao gerenciador de dados
- [ ] Testar com dados próprios e legalmente obtidos, caso disponíveis
- [ ] Registrar FPS e quaisquer erros sem expor credenciais

## Compilação
- [ ] Escolher MSYS2 MINGW64 ou Visual Studio 2022 + vcpkg
- [ ] Instalar dependências de `BUILD_INSTRUCTIONS.md`
- [ ] Configurar CMake em pasta de build, sem modificar arquivos versionados
- [ ] Compilar e executar testes
- [ ] Conferir `git status` antes de publicar alterações

## Privacidade e distribuição
Não publicar tokens, senhas, dados pessoais sensíveis nem recursos proprietários do jogo.
