# Rodando no PS5 (firmware 13.60, Relapse + etaHEN)

O núcleo (`src/core`) já é independente de plataforma: só usa C, `math.h` e sockets BSD.
No PS5 falta escrever a camada `src/ps5/`, compilada com o
[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk).

## Arquitetura planejada

```
┌───────────────────── daemon gyroaim.elf (payload) ─────────────────────┐
│  • detecta o jogo em primeiro plano (title id) → ga_app_set_title()     │
│  • servidor web na porta 8090 → mesma página dos sliders                │
│  • guarda perfis em /data/gyroaim/profiles/<TITLE_ID>.ini               │
│  • notificações na tela (sceKernelSendNotificationRequest)              │
└──────────────┬──────────────────────────────────────────────────────────┘
               │ injeta (ptrace) quando um jogo abre
┌──────────────▼───── hook dentro do processo do jogo ────────────────────┐
│  scePadReadState / scePadRead do jogo:                                  │
│    1. chama a função original                                           │
│    2. lê angularVelocity do mesmo estado (liga o sensor se preciso)     │
│    3. ga_update() → escreve rightStick, remove botões de atalho         │
│    4. devolve o estado modificado ao jogo                               │
└─────────────────────────────────────────────────────────────────────────┘
```

A config é passada do daemon para o hook por memória compartilhada ou socket local,
então trocar de jogo ou mexer num slider vale na hora.

## Etapas e riscos

| Etapa | Situação |
|---|---|
| Núcleo, perfis, página web | **pronto** e testado no Linux |
| Daemon no PS5: web + perfis + notificação | viável, o SDK já tem exemplos de sockets e notificação |
| Detectar o title id do jogo aberto | viável, mas a chamada exata do sistema precisa ser confirmada no console |
| **Hook do controle dentro do jogo** | **parte mais difícil**: injetar código no processo do jogo e trocar a entrada de `scePadReadState` na tabela de imports. Ferramentas da cena já usam hooks de `scePad*`, mas precisamos validar no 13.60 |
| Atalho na barra de menu (Central de Controle) | **não previsto por enquanto**: exigiria modificar o ShellUI do sistema, que é frágil e muda entre firmwares. Alternativas: atalhos no controle e a página web (dá para fixar no celular) |

## Observações

- Não use o console desbloqueado online na PSN. Em jogos online, alterar a entrada do
  controle também pode ser tratado como trapaça.
- Alguns jogos leem o controle por outras funções (`scePadRead` com buffer, engines com
  módulos próprios). O hook vai precisar de ajustes por engine/jogo.
