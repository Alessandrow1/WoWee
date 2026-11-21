# World of Legends — conexao externa por dois tuneis TCP

Objetivo: o host roda AzerothCore e um agente de tunel; o convidado instala apenas o cliente World of Legends.

## Pre-requisitos

- Servidor AzerothCore local funcionando: authserver em TCP 3724 e worldserver em TCP 8085.
- Dois tuneis TCP publicos com encaminhamento ao computador do host:
  1. AUTH: host publico A, porta publica A -> 127.0.0.1:3724.
  2. WORLD: host publico B, porta publica B -> 127.0.0.1:8085.
- O fornecedor de tuneis deve disponibilizar encaminhamento TCP arbitrario. **Planos gratuitos podem restringir TCP**, e as portas publicas podem ser diferentes das portas locais.
- Nao exponha MySQL (3306), SOAP, console remoto nem qualquer painel de administracao.

## Configurar cliente convidado

No arquivo `World-of-Legends.ini`, na secao `[WorldOfLegends]`:

```ini
ServerHost=HOST_PUBLICO_AUTH
ServerPort=PORTA_PUBLICA_AUTH
WorldHost=HOST_PUBLICO_WORLD
WorldPort=PORTA_PUBLICA_WORLD
```

O launcher envia `ServerHost` e `ServerPort` ao wowee.exe, que seleciona o perfil de autenticacao correspondente. Os campos `WorldHost`/`WorldPort` sao usados para testar o status no launcher, **nao** controlam o destino do protocolo do jogo.

## Configurar realmlist do servidor

No MySQL, com backup previo, consulte primeiro:

```sql
SELECT id, name, address, localAddress, localSubnetMask, port FROM acore_auth.realmlist;
```

Quando tiver os dois enderecos reais do provedor de tunel, configure APENAS o reino desejado (exemplo para ID 1):

```sql
UPDATE acore_auth.realmlist
   SET address = 'HOST_PUBLICO_WORLD',
       port = PORTA_PUBLICA_WORLD
 WHERE id = 1;
```

IMPORTANTE: `address`/`port` apontam para o endpoint **WORLD**, nao para o endpoint AUTH. A porta publica do tunel WORLD pode ser diferente de 8085.

`localAddress` e `localSubnetMask` devem ser mantidos de acordo com a topologia existente. Alguns acessos locais podem receber `localAddress`, ao passo que convidados externos recebem `address`.

## Testes

1. Host inicia MySQL, authserver, worldserver e o agente do tunel.
2. Host testa primeiro as portas locais com `verificar-host-multiplayer.ps1`.
3. Confirme que o agente do tunel informa ambos os endpoints publicos como ativos.
4. No PC convidado, configure `ServerHost`/`ServerPort` no launcher ou no INI, e `WorldHost`/`WorldPort` no INI.
5. Teste login e entrada no mundo, de outra rede. Um status TCP online nao garante que o protocolo do jogo esteja correto.
6. Se o login funciona, mas falha ao entrar no mundo, confira `realmlist.address` e `realmlist.port` antes de alterar o cliente.

## Limitacoes

Esta etapa nao cria automaticamente tuneis nem cadastro de contas. A autenticacao do jogo nao substitui um cadastro seguro com HTTPS. O cliente exige dados de jogo obtidos legalmente pelo jogador. Nao distribua assets proprietarios sem autorizacao.

Ver tambem: https://www.azerothcore.org/wiki/realmlist
