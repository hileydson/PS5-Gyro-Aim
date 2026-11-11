# Estado da integração PS5

O projeto oferece um caminho de entrada via PC + Chiaki/Remote Play, descrito em
[REMOTE_PLAY.md](REMOTE_PLAY.md). Ele evita injeção de código nos jogos e mudanças
na Central de Controle. O console não precisa executar código deste projeto para
o modo manual ou para a descoberta de título pela rede, quando ela estiver disponível.

## Payload opcional de painel e identificação

`src/ps5/main.c` implementa um executável independente que:

- Consulta o jogo aberto a cada 500 ms com `sceSystemServiceGetAppIdOfRunningBigApp`
  e `sceSystemServiceGetAppTitleId`, seguindo as assinaturas do
  [exemplo Injector do etaHEN](https://github.com/etaHEN/etaHEN-Plugins/blob/6339554e4e3c92c0a655a37194c1152d6b82ad88/Plugin_samples/Injector/source/main.cpp).
- Aceita Title IDs `PPSA`/`CUSA` seguidos de cinco números, após duas leituras estáveis.
- Volta ao padrão se as consultas falharem de forma persistente.
- Serve o painel na porta 8090, usando `/data/gyroaim/profiles`.
- Envia notificações comuns via `sceKernelSendNotificationRequest`.
- Fecha sockets ao receber SIGINT/SIGTERM.

Ele **não lê o DualSense nem altera a entrada dos jogos**. O estado do painel
informa a ausência de sensor/saída. Para jogar com gyro, o fluxo Remote Play roda
no PC. O iniciador pode consultar este payload como fonte de Title ID:

```bash
python3 tools/play_ps5.py --ps5-panel http://IP-DO-PS5:8090
```

Nesse modo abra o painel do **PC** no celular para ajustar a mira aplicada pelo
virtual. Alterações no painel do PS5 são gravadas no console, em perfis separados.

## Compilação

Use o [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk):

```bash
make ps5 PS5_PAYLOAD_SDK=/caminho/ps5-payload-sdk
```

Resultado: `build/gyroaim-ps5-panel.elf`. Compilação cruzada realizada com SDK
**v0.45** e LLVM **19.1.1**. É um payload ELF, **não** um `.plugin` etaHEN.
O target não transmite o arquivo ao console e não instala inicialização automática.
A execução depende de um carregador ELF compatível e das bibliotecas do firmware.

A compilação confirma resolução de símbolos e formato ELF; não confirma que essas
APIs, notificações ou o ciclo de vida funcionem no firmware 13.60. Não foi executado
no PS5 nesta implementação. A integração deve ser testada pelo caminho de carga
já usado no console, começando somente por consulta de título e painel.

## O que continua pendente no modo autônomo, sem PC

| Parte | Estado |
|---|---|
| Conversão gyro → analógico, perfis e painel | Implementada e testada no host |
| Payload com servidor e consulta de título | Compilado; execução física pendente |
| Receber entrada dentro dos jogos PS5 | Não implementado |
| Hook seguro em `scePadReadState` e `scePadRead` | Não implementado/validado |
| Cobrir chamadas de controle em módulos de cada jogo | Não implementado/validado |
| Comunicação daemon ↔ hook, ciclo de suspensão/retorno | Não implementada |
| Overlay sobre jogo / Central de Controle | Não implementado |
| Compatibilidade universal no console | Não certificada |

Um modo totalmente autônomo exigiria essas etapas e testes por firmware/jogo.
Modificar o ShellUI não é necessário para o painel no celular, e não foi usado
como atalho para oferecer uma aparência de integração pronta.
