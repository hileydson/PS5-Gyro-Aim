# PS5 Gyro Aim

Use o **giroscópio do DualSense para mirar**: o movimento do controle vira o analógico
direito, como o "gyro aim" de Switch/Steam Deck, em qualquer jogo.

O alvo final é um PS5 desbloqueado (firmware 13.60, Relapse + etaHEN). Hoje o projeto já
funciona no **PC (Linux)**, onde dá para testar e ajustar tudo com o DualSense de verdade.
O núcleo é o mesmo código que vai rodar no console.

## Recursos

- **Botão de ativação** configurável: qualquer botão (padrão L2), no modo *segurar*,
  *liga/desliga* ou *sempre ligado*.
- **Sensibilidade** e **anti-deadzone** com sliders, além de ajustes avançados:
  sensibilidade vertical, suavização, filtro de tremor e inverter eixos.
- **Perfil por jogo**: cada jogo guarda a própria configuração. Ao abrir um jogo, o perfil
  dele é carregado sozinho. Jogos novos usam o perfil padrão.
- **Atalhos no controle**, sem sair do jogo (segure o clique do Touchpad):

  | Botão | Ação |
  |---|---|
  | ↑ / ↓ | sensibilidade ±0.1 |
  | → / ← | anti-deadzone ±2% |
  | △ | liga/desliga o gyro neste jogo |

  Cada mudança é salva no perfil do jogo e mostra uma mensagem.
- **Página de configuração** no navegador (celular, PC ou o navegador do PS5), com o
  analógico virtual ao vivo.
- **No PC:** um controle virtual (Xbox 360) para usar o gyro em qualquer jogo ou app.

## Status

| Parte | Situação |
|---|---|
| Núcleo gyro → analógico, perfis, atalhos | ✅ pronto, com testes |
| Página de configuração | ✅ pronta |
| Simulador Linux + controle virtual | ✅ pronto e testado com DualSense |
| Versão PS5 (payload + hook no jogo) | 🚧 em desenvolvimento, veja [docs/PS5.md](docs/PS5.md) |

## Como funciona

```
giroscópio (°/s) ─► deadzone do tremor ─► × sensibilidade ─► suavização
                                                              │
analógico direito final ◄─ soma com o stick físico ◄─ anti-deadzone
```

- **Sensibilidade 1.0** = girar o controle a 100 °/s leva o analógico ao máximo.
- **Anti-deadzone**: os jogos ignoram movimentos pequenos do analógico (zona morta). O
  anti-deadzone faz qualquer movimento do gyro começar já fora dessa zona, então ajustes
  finos de mira funcionam. Aumente se movimentos pequenos não mexem a mira, diminua se a
  mira "pula".
- **Deadzone do gyro**: ignora o tremor natural da mão, para a mira não andar sozinha.

## Estrutura

```
src/core/     núcleo portátil, o mesmo código no Linux e no PS5
  gyro_aim.*  gyro → analógico, ativação, anti-deadzone, atalhos
  profile.*   perfis <TITLE_ID>.ini por jogo + default.ini
  app.*       estado da aplicação, troca de jogo, mensagens
  webui*      servidor HTTP mínimo + página com os sliders
src/linux/    simulador: lê o DualSense pelo evdev e cria o controle virtual (uinput)
tests/        testes do núcleo (não precisam de controle)
docs/PS5.md   arquitetura da versão PS5 e o que falta
```

## Testar no PC (Linux)

Requisitos: `gcc` e `make`. Não há outras dependências.

```bash
make test
```

```bash
make run
```

1. Conecte o DualSense por USB ou Bluetooth.
2. Abra http://localhost:8090 e ajuste os sliders.
3. Segure **L2** (o padrão) e gire o controle: o `@` no terminal é o analógico virtual.
4. Teclas `1`–`4` simulam abrir jogos diferentes, e `0` volta para "sem jogo". Configure
   algo diferente em cada um e alterne para ver o perfil trocar.

Opções: `--port 8090`, `--profiles ./profiles`, `--no-virtual`, `--no-grab`.

### Controle virtual (usar em qualquer jogo/app do PC)

Enquanto o simulador roda, ele cria o **"Gyro Aim Virtual Controller"**, que o sistema vê como
um controle de Xbox 360. Ele recebe os botões, gatilhos e o analógico esquerdo do DualSense, e
o analógico direito já vem com o gyro aplicado. O DualSense real fica escondido dos apps que
leem o evdev (`--no-grab` desativa isso, `--no-virtual` desliga o controle virtual).

- **Steam** lê o DualSense direto pelo hidraw, então pode ver os dois controles. Desative
  "Suporte a controle PlayStation" nas configurações de controle da Steam, ou escolha o
  controle virtual no jogo.
- Sem permissão em `/dev/uinput`, o terminal mostra "ERRO ao criar". Rode com `sudo` ou
  adicione uma regra udev para o seu usuário.
- Se a mira andar ao contrário, use "Inverter horizontal/vertical" nos ajustes avançados.

## Perfis

Cada jogo vira um arquivo `profiles/<TITLE_ID>.ini` (no PS5: `/data/gyroaim/profiles/`):

```ini
enabled=1
activation=L2
toggle_mode=0
sensitivity=1.50
anti_deadzone=0.20
vertical_ratio=0.80
smoothing=0.25
gyro_deadzone=1.00
invert_x=0
invert_y=0
```

## Aviso

Projeto de homebrew para o seu próprio console. Não use um console desbloqueado online na
PSN. Em jogos online, alterar a entrada do controle pode ser tratado como trapaça.
