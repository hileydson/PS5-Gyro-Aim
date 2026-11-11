# PS5 Gyro Aim

Converte o giroscópio do DualSense em analógico direito, com perfis por jogo e um
painel para ajustar a mira durante a partida.

**Fluxo recomendado para o PS5:** DualSense no PC Linux → controle virtual →
Chiaki/chiaki-ng → Remote Play local. Você continua olhando o jogo na TV e usa o
painel no celular. Esse caminho evita injetar código nos jogos e modificar o menu
do sistema do console. Precisa de um PC ligado e de Remote Play já pareado.

Isso não significa compatibilidade universal comprovada: o jogo deve permitir
Remote Play e usar o analógico direito para mirar. O comportamento da câmera,
limite de giro e zona morta continuam dependendo do jogo. O fluxo completo no PS5
13.60 ainda exige teste no console; o código e os testes locais não o certificam.

## O que está implementado

- Perfis independentes, salvos em `profiles/<TITLE_ID>.ini`, com padrão para jogos novos.
- Painel responsivo no celular, com assistente de seis etapas por jogo, presets iniciais
  e ajustes sugeridos por sintoma: mira parada, saltos, lentidão, excesso de movimento e tremor.
- Ajustes rápidos no controle e no painel: sensibilidade, compensação de zona morta,
  ativação, proporção vertical, suavização, filtro de tremor e inversão de eixos.
- Calibração do sensor parado por 2 segundos, com rejeição de amostras instáveis,
  cancelamento e preservação do ajuste anterior se houver erro ao salvar.
- Pausa temporária do gyro, desfazer o último ajuste e marcar um ponto de retorno para
  comparar várias alterações no mesmo jogo. Pausa e ponto de retorno valem na sessão.
- Painel compacto em `/compact`, para uma janela separada no PC. Não é um overlay da TV.
- Iniciador para Chiaki que filtra os controles via SDL para evitar entrada duplicada.
- Detecção opcional do título via descoberta do Remote Play na rede local, sem payload.
  Duas leituras estáveis confirmam a troca. Sem metadados válidos, suspende o gyro.
- Payload PS5 opcional que consulta o título e serve o painel; compilado com o SDK v0.45,
  ainda sem validação física. Não faz hook, ptrace, patches de jogo ou do ShellUI.
- HTTP sem espera por clientes lentos; alterações de várias chaves são atômicas e
  exigem título/revisão atuais para evitar aplicar uma solicitação ao jogo errado.

## Compilar e testar

Requisitos Linux: compilador C, `make` e Python 3.

```bash
make
make test test-http
```

Os testes usam entradas sintéticas, pipes e diretórios temporários; não exigem
controle, PS5, Chiaki ou permissão no uinput. Não substituem o teste real de mira.

## Treino local no Linux, sem PS5

```bash
make
./build/gyroaim-linux --title TREINO_LINUX
```

Abra [o treino de mira](http://localhost:8090/practice) no navegador deste PC:

1. Pressione **X no DualSense** para o navegador liberar o controle. Se necessário,
   escolha **Gyro Aim Virtual Controller** na lista de controles do treino.
2. Segure **L2** e mova o controle para mirar. O analógico direito também move a câmera.
3. Aperte **X ou R2** para acertar os círculos.
4. Ajuste sensibilidade, compensação e calibração no painel ao lado e teste de novo.
5. A opção **Zona morta do treino** simula a configuração interna de um jogo; não
   altera a compensação salva no seu perfil.

O treino lê o controle virtual por `navigator.getGamepads()`, sem usar a saída
calculada do painel como substituta. Por isso verifica o caminho que um jogo vê.
Abra-o em **localhost no PC**, com a aba em foco. No celular, use apenas o painel
para configurar. Há uma opção explícita de **demonstração por teclado** (setas e
Espaço), que testa os alvos, mas não o gyro nem a saída virtual.

Para repetir a verificação automática com o dispositivo real do Linux:

```bash
make test-linux
```

Este teste adicional precisa de acesso a `/dev/uinput` e aos eventos em
`/dev/input`. Ele cria e remove seu próprio controle sintético, envia movimentos
conhecidos pelo núcleo e confere os valores recebidos via evdev. Não captura o
DualSense físico. `make test test-http` continua independente de dispositivos.

## Usar com jogos do PC

```bash
make run
# ou um perfil específico:
./build/gyroaim-linux --title MEU_JOGO
```

1. Conecte o DualSense ao PC por USB ou Bluetooth.
2. Abra [o painel](http://localhost:8090). No celular, abra `http://IP-DO-PC:8090` na mesma rede.
3. Selecione o perfil do jogo, abra o assistente e ajuste enquanto testa a mira.
4. Por padrão, segure L2 para ativar o gyro.

O aplicativo cria **Gyro Aim Virtual Controller**, compatível com Xbox 360. O jogo
precisa aceitar esse controle. Steam/SDL podem ler o DualSense por hidraw mesmo com
o grab de evdev; evite usar o controle físico e o virtual simultaneamente no jogo.

Opções: `--port 8090`, `--profiles ./profiles`, `--title JOGO`, `--no-virtual`,
`--no-grab`, `--no-controller` (painel sem acessar dispositivos) e `--remote-play`.
O teclado do terminal mantém `1`–`4` para perfis de teste e `0` para o padrão.

## Usar com o PS5 sem hooks

Pareie primeiro o seu console no Chiaki/chiaki-ng. O iniciador usa esse pareamento
existente; não lê nem altera credenciais. Veja os detalhes e limites em
[docs/REMOTE_PLAY.md](docs/REMOTE_PLAY.md).

```bash
# Perfil manual, sem depender de detecção do título:
python3 tools/play_ps5.py --title PPSA01234

# Tenta acompanhar os jogos pelo IPv4 do PS5 na rede local:
python3 tools/play_ps5.py --console 192.168.1.50

# Somente mostrar comandos; não inicia nem captura o controle:
python3 tools/play_ps5.py --dry-run --console 192.168.1.50
```

O iniciador abre o Chiaki e o painel fica no PC. Conecte a sessão no Chiaki e
continue vendo a imagem na entrada HDMI do PS5 na TV. No celular, o painel é
`http://IP-DO-PC:8090`, e não o IP do console. Se já houver uma sessão do Gyro Aim
rodando, encerre-a ou use outra porta **e libere o controle da sessão anterior**.

O modo automático precisa que o console publique o identificador do jogo. Consulte
isso sem iniciar sessão nem acordar o console:

```bash
python3 tools/ps5_discovery.py 192.168.1.50
```

Sem o título publicado, use seleção manual. O payload opcional é outra fonte de
identificação, descrita em [docs/PS5.md](docs/PS5.md).

## Ajustar jogando

1. Abra o jogo e confira o nome/Title ID no painel. Perfis novos abrem o assistente.
2. Em uma área tranquila, selecione a ativação e calibre com o controle apoiado.
3. Ajuste a compensação até movimentos pequenos começarem a mover a mira sem saltos.
4. Ajuste a sensibilidade acompanhando um alvo; depois refine tremor e suavização.
5. Marque um ponto de retorno, experimente e escolha manter ou voltar. Alterações
   já são salvas automaticamente no perfil exibido.

Com os atalhos habilitados, segure o **clique do Touchpad**:

| Botão | Ação |
|---|---|
| ↑ / ↓ | Sensibilidade ±0,1 |
| → / ← | Compensação de zona morta ±2% |
| △ | Liga/desliga o gyro neste jogo |
| □ | Mostra lembrete dos atalhos no painel |

Desative os atalhos por perfil se conflitarem com o jogo. O controle virtual Xbox
não transmite toques/deslizamentos do touchpad, áudio, microfone, háptica ou gatilhos
adaptáveis. Use os atalhos de teclado/mapeamento do seu Chiaki para as funções de
PlayStation que não estejam disponíveis no virtual. Isso limita alguns jogos.

## Perfis e recuperação

Os arquivos anteriores continuam compatíveis. Novas chaves:

```ini
hotkeys_enabled=1
bias_pitch=0.0000
bias_yaw=0.0000
```

A gravação usa arquivo temporário, flush, sincronização e renomeação. Se falhar,
o ajuste anterior continua ativo e o painel mostra o erro. Desfazer e pontos de
retorno não sobrevivem ao encerramento do aplicativo ou à troca de jogo.
O progresso do assistente fica neste navegador, separado por Title ID.

No final do painel completo e compacto, **Restaurar default deste perfil** recupera
os valores de fábrica do aplicativo (incluindo calibração), salva somente o
perfil aberto e permite desfazer. Para recuperar seu padrão personalizado, use
**Copiar padrão para este jogo** no painel completo.

O painel é um serviço para rede local confiável, sem autenticação. Não exponha a
porta 8090 à internet. Não é necessário atualizar o console ou conectá-lo à PSN
para testar o código local; a disponibilidade do pareamento Remote Play depende
da configuração atual do seu console.

## Estrutura

```
src/core/    conversão gyro, perfis, estado, servidor e painel portátil
src/linux/   DualSense via evdev + controle virtual via uinput
src/ps5/     payload opcional de painel e consulta de título (sem entrada gyro)
tools/       iniciador Chiaki, descoberta de título e gerador da página C
tests/       núcleo, controle virtual, HTTP e descoberta/iniciador
```

Edite `src/core/webui.html`; `make` gera `webui_page.c`. A página não usa CDN ou
bibliotecas externas. [docs/PS5.md](docs/PS5.md) separa o que foi compilado do que
precisa ser validado no hardware.
