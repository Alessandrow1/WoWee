# Nosso cliente WoW — início do desenvolvimento

Esta pasta organiza uma derivação **não comercial** do [WoWee](https://github.com/Kelsidavis/WoWee). Nome provisório: **Nosso Cliente WoW**.

## Princípios

- Alvo principal: Windows 11 x64.
- Preservar C++, CMake, SDL3 e Vulkan como base.
- Preservar atribuições, `LICENSE` e `NOTICE` originais.
- Não reutilizar músicas originais do WoWee nem distribuir dados proprietários de World of Warcraft.
- Não desenvolver mecanismos de burla de controles de servidores.
- Desenvolver em `develop`; manter `master` como referência estável até definir política de releases.

## Primeiro marco

1. Validar GPU, Vulkan e ferramentas em Windows.
2. Testar a execução da versão oficial do WoWee (baseline).
3. Instalar toolchain e compilar o cliente original sem alterações.
4. Definir identidade visual própria e implementar melhorias em mudanças pequenas.

Veja [ROADMAP.md](ROADMAP.md) e [TESTES_WINDOWS.md](TESTES_WINDOWS.md).
