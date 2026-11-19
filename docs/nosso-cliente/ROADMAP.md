# Roadmap inicial

## M0 — Fundamentos
- [x] Fork criado e `origin` validado
- [x] Branch `develop` criada no GitHub
- [x] GPU/driver identificados; Vulkan 1.4.312 relatado pelo driver
- [ ] Testar executável oficial da release no Windows
- [x] Instalar CMake, compilador, `glslc` e dependências conforme o guia oficial (MSYS2 MINGW64 na E:)
- [x] Compilar o cliente original no Windows e executar o `wowee.exe` (relatado como aberto normalmente)
- [ ] Instalar StormLib e habilitar o gerenciador/extrator de assets (opcional até usar dados de jogo)

## M1 — Diferenciação
- [ ] Nome e identidade visual próprios
- [x] Traduzir a tela de login e painel gráfico para pt-BR (compilação e inspeção visual pendentes)
- [ ] Ampliar localização pt-BR para outras telas e mensagens
- [ ] Melhorias no painel de configurações (além da tradução)
- [ ] Perfil gráfico conservador para placas de entrada

## M2 — Robustez
- [ ] Registrar FPS e erros localmente
- [ ] Testes automatizados das mudanças
- [ ] Testar regressões de login, mundo, áudio, interface e addons

## M3 — Distribuição
- [ ] Build Windows x64 reproduzível
- [ ] ZIP com licenças, documentação e recursos autorizados
- [ ] Não incluir arquivos proprietários da Blizzard nem músicas de licença separada

**Observação:** suporte à API Vulkan não garante todos os recursos exigidos nem FPS satisfatório.
