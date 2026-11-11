> Experimento auxiliar de laboratório. O produto principal é o app homebrew
> nativo instalado no PS5, sem PC/Chiaki/Remote Play durante o uso. Este documento
> preserva a experiência anterior; não substitui a integração nativa descrita em
> [PS5.md](PS5.md).

# Jogar na TV e ajustar pelo celular

O caminho com menor intervenção no console é usar a entrada do Remote Play:

```
DualSense → PC / Gyro Aim → controle virtual Xbox → Chiaki → PS5 → jogo na TV
                ↑
     navegador no celular: perfis e assistente
```

A imagem pode continuar na TV pelo HDMI do PS5. O PC mantém uma sessão do Chiaki
para transmitir comandos. O painel não se sobrepõe à imagem HDMI; abre no celular
ou em uma janela separada no PC. Não foi implementada modificação da Central de
Controle, overlay injetado, hook em jogos ou escrita em memória de processos.

A documentação do [Chiaki sobre controles](https://github.com/chiaki-ng/chiaki-ng/blob/main/docs/setup/controlling.md)
descreve gyro como analógico para jogos sem suporte nativo. Aqui o Gyro Aim faz a
conversão antes de entregar os eixos ao Chiaki. A integração ainda precisa ser
validada ponta a ponta no PS5 do usuário.

## Preparação

- Um PC Linux com DualSense e permissão para ler `/dev/input` e escrever `/dev/uinput`.
- Chiaki ou chiaki-ng instalado e o PS5 já registrado nele para conexão local.
- Remote Play habilitado no console; PC, PS5 e celular na rede local.
- Um jogo que permita Remote Play e use o analógico direito para a câmera/mira.

O [guia de configuração do Chiaki](https://github.com/chiaki-ng/chiaki-ng/blob/main/docs/setup/configuration.md)
explica o pareamento. O iniciador não faz login, não cadastra conta, não busca
Account ID, não atualiza firmware e não acorda o console. Use as condições de
pareamento disponíveis no seu console; não há confirmação de compatibilidade do
firmware 13.60 com esta implementação.

## Iniciar

Na raiz do projeto:

```bash
make
python3 tools/play_ps5.py --title PPSA01234
```

O programa espera pelo sensor e pelo controle virtual, abre o cliente disponível
e mantém ambos até você fechar o Chiaki ou usar Ctrl+C. O log do Gyro Aim fica em
`build/remote-play.log`. Ele não encerra outras sessões já abertas.

O SDL do cliente é configurado somente no processo filho:

```text
SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x045e/0x028e
SDL_JOYSTICK_HIDAPI=0
SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1
```

O formato da lista de VID/PID vem da
[documentação do SDL](https://wiki.libsdl.org/SDL2/SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT).
Ela seleciona a família Xbox 360 do controle virtual; outro controle real com o
mesmo VID/PID também passaria pelo filtro. Desconecte controles Xbox adicionais.
No cliente, confira que apenas o controle virtual está sendo usado. O filtro é
um auxílio, sem confirmação automática de que a sessão do Chiaki está conectada.

Para escolher outro executável ou passar parâmetros próprios do seu cliente:

```bash
python3 tools/play_ps5.py --client /caminho/chiaki-ng --title PPSA01234 -- ARGUMENTOS-DO-CLIENTE
```

Se o cliente estiver instalado por Flatpak, o iniciador tenta os IDs conhecidos.
O sandbox do pacote precisa permitir leitura de controles; o programa não altera
permissões do Snap/Flatpak automaticamente.

Abra `http://IP-DO-PC:8090` no celular. No PC, `/compact` abre uma versão menor do
painel em outra janela. Ela não tem garantia de ficar sempre por cima ou aparecer
sobre jogos em tela cheia.

## Troca automática de jogo sem payload

```bash
python3 tools/ps5_discovery.py 192.168.1.50
python3 tools/play_ps5.py --console 192.168.1.50
```

O console recebe apenas uma consulta `SRCH` por UDP na porta 9302, sem credenciais
ou comando de wakeup. O parser segue os campos do
[código de descoberta do Chiaki](https://github.com/chiaki-ng/chiaki-ng/blob/main/lib/src/discovery.c):
`running-app-titleid`, `running-app-name` e versão de protocolo `00030010`.

Duas leituras consecutivas do mesmo título confirmam a troca (aproximadamente um
segundo mais o tempo de resposta). O perfil salvo é aplicado e o painel mostra o
nome/identificador. Ao receber uma falha de consulta, o iniciador seleciona o perfil
padrão: **neste modo o gyro fica suspenso quando não há um jogo identificado**;
os comandos físicos seguem disponíveis. A detecção retorna após duas leituras
válidas. Os valores do perfil padrão continuam editáveis para jogos futuros.

O protocolo pode omitir o título e não distingue necessariamente a Central de
Controle de um jogo ainda aberto. Não inferimos o jogo pelo nome nem tratamos
metadados ausentes como compatibilidade. Sem metadados, inicie **sem `--console`**
e escolha manualmente o perfil no painel. Com a detecção funcionando, uma seleção
manual fica ativa até a próxima troca detectada; o painel informa a origem.

Como alternativa, use o payload de consulta descrito em [PS5.md](PS5.md):

```bash
python3 tools/play_ps5.py --ps5-panel http://192.168.1.50:8090
```

Nesse caso o perfil/gyro ativo continua no PC; o servidor do PS5 é apenas a fonte
de identificação, e não envia comandos ao jogo. Os diretórios de perfis dos dois
servidores são independentes.

## Teste dentro do jogo

1. Entre no treino ou em uma área sem combate e confira o perfil exibido.
2. Comece com L2, segurar, sensibilidade baixa e poucos efeitos de aceleração no jogo.
3. Calibre o sensor com o controle numa superfície parada.
4. Use o assistente do painel para encontrar a zona morta e a sensibilidade.
5. Marque um ponto de retorno antes de experimentar vários ajustes.
6. Alterne entre dois jogos e confirme que cada perfil volta com seus próprios valores.
7. Desconecte o controle e reconecte: verifique que a saída não fica presa.

Não certificamos “qualquer jogo”: restrições do Remote Play, jogos com periféricos,
VR, touchpad obrigatório ou curvas de câmera incomuns podem impedir ou limitar o
uso. O virtual não encaminha háptica, gatilhos adaptáveis, sensores nativos,
touchpad ou áudio; a conversão de gyro em analógico continua limitada pela velocidade
máxima da câmera do jogo. Há também latência de entrada da rede, mesmo vendo a TV.

## Verificação realizada

- Testes automatizados do núcleo, calibração, cancelamento, perfis, desfazer e pausa.
- Testes dos eventos do virtual por pipe: botões não ficam presos ao usar atalhos.
- HTTP real: conflitos de perfil/revisão, corpo fragmentado, cliente lento e falha de gravação.
- Testes do parser de descoberta, estabilização e filtro de controles do iniciador.
- SDL local reconheceu o controle virtual como `X360 Controller`.
- Interface verificada no navegador.

Pendente: pareamento/sessão real do Chiaki, resposta de descoberta do PS5 13.60,
validação da mira por jogo e medição da latência no equipamento do usuário.
