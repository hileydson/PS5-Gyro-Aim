# PS5 Gyro Aim

**O produto é um aplicativo homebrew nativo para instalar no PS5.** O Linux é uma
bancada de desenvolvimento e testes do mesmo núcleo, não uma dependência para jogar.
O alvo é DualSense conectado ao PS5, perfis salvos no console, ajustes no app pela
TV e painel opcional no celular em `http://IP-DO-PS5:8090`.

O núcleo, os perfis, o painel e a aplicação de configuração já estão implementados.
A aplicação do gyro à entrada dos jogos PS5 **ainda não está implementada/validada**.
Instalar a build atual permite testar configuração e comunicação; não habilita
sozinho gyro nos jogos. Compatibilidade universal não está certificada.

## Arquitetura no console

```
App Gyro Aim (ícone no menu do PS5)     Celular (opcional)
         │ configuração por controle        │ navegador
         └──────────────┬─────────────────────┘
                        ▼
       Serviço Gyro Aim do etaHEN, no próprio PS5
       perfis em /data/gyroaim/profiles + painel HTTP
                        │
                        ▼
       Backend de entrada dos jogos (ainda pendente)
                        │
                        ▼
                 Jogo no próprio PS5
```

- **App nativo:** `eboot.bin`, com interface gráfica, navegação pelo DualSense,
  ajustes de mira, perfis, calibração, ponto de retorno, desfazer e restaurar default.
- **Serviço residente:** `.plugin` do etaHEN. Guarda e serve os mesmos perfis para
  o app e o celular. Fechar o app não é um comando para encerrar o serviço.
- **Backend de entrada:** precisa integrar o gyro às leituras do controle dos jogos.
  Sem essa parte, o serviço informa que a saída para jogos está indisponível.

Não se exige PC, Chiaki ou Remote Play na arquitetura final do console.
O experimento anterior de Remote Play foi mantido em [docs/REMOTE_PLAY.md](docs/REMOTE_PLAY.md)
como material auxiliar de laboratório; não é o caminho principal do produto.

## Gerar a build PS5

Requisitos de desenvolvimento: Python 3.12+, compiladores C/C++, LLVM/Clang/LLD,
zlib de desenvolvimento e [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk).
As ferramentas de build rodam no PC; os binários resultantes rodam no console.

```bash
# Dependências públicas com versões e SHA-256 fixados:
make ps5-deps

# Aplicativo nativo + serviço residente:
make ps5 PS5_PAYLOAD_SDK=/caminho/ps5-payload-sdk
```

Saídas:

```
build/ps5/
  homebrew/PPSA94001/
    eboot.bin                  aplicação nativa em contêiner FSELF
    sce_module/libc.prx        runtime aberto gerado pelo SDK
    sce_sys/param.json         metadados do aplicativo Gyro Aim
    sce_sys/icon0.png          ícone
  etaHEN/plugins/
    gyroaim-service.plugin     serviço residente, ID GYRO00001
  manifest.json                componentes, hashes e limites da build
```

É uma **pasta homebrew nativa**, não um arquivo renomeado para `.pkg`. O registro
na tela inicial depende do carregador compatível do console, como o formato
usado pelo ShadowMount+. O serviço usa o mecanismo de plugins do etaHEN.
Veja [docs/PS5.md](docs/PS5.md) para instalação e limites.
O build também gera `build/GyroAim-PS5-experimental.zip`, contendo somente a
pasta do aplicativo, o plugin, o manifesto e as instruções de validação.

`make ps5-service` gera somente o serviço. Os caminhos de dependências podem ser
substituídos por `PS5_NATIVE_SDK` e `PS5_NATIVE_DEPS`. O build não instala nada,
não transmite arquivos ao PS5 e não altera o firmware.

## Testar sem o PS5

```bash
make
make test test-http test-native
```

Esses testes não precisam de controle, PS5 ou uinput. Cobrem conversão, perfis,
calibração, desfazer, restauração, falhas de gravação, HTTP e comunicação do app
nativo com o serviço. `make test-linux` acrescenta um teste real de uinput,
criando/removendo apenas seu próprio dispositivo sintético.

### A mesma interface nativa em laboratório

Com as dependências de `make ps5-deps` disponíveis e o runtime SDL2 do Linux:

```bash
make native-lab
# Em um terminal: serviço de laboratório, sem acessar controles:
./build/gyroaim-linux --no-controller --no-virtual --title TESTE_NATIVO
# Em outro terminal: mesma interface de configuração compilada para Linux:
./build/gyroaim-native-lab
```

D-pad ou setas navegam; esquerda/direita alteram valores; X/Enter aplica; L1/R1
ou PageUp/PageDown mudam de aba. Círculo/Esc fecha a interface. A interface se
comunica por localhost com o serviço que possui os perfis; não mantém cópias de
configuração em outro diretório. `--preview imagem.bmp` exporta uma prévia gráfica
sem abrir janela nem acessar controles.

### Treino da conversão com DualSense

```bash
./build/gyroaim-linux --title TREINO_LINUX
```

Abra [o treino local](http://localhost:8090/practice). Aperte X para o navegador
reconhecer **Gyro Aim Virtual Controller**. Segure L2 e gire para mirar; X ou R2
atira. O painel ao lado permite ajustar enquanto testa. A demonstração por teclado
é separada e não testa o gyro. Esse treino valida o núcleo e a saída uinput do
laboratório; não emula nem certifica as APIs de entrada do PS5.

## Configurações e perfis

Cada Title ID possui um `.ini`; `default.ini` é o padrão personalizado para jogos
novos. O núcleo oferece ativação por botão/sempre ligado, segurar/alternar,
sensibilidade, compensação de zona morta, proporção vertical, suavização, filtro
de tremor, inversão de eixos, calibração e atalhos opcionais.

**Restaurar default deste perfil** recupera os valores de fábrica somente no
perfil aberto, incluindo calibração, e pode ser desfeito. **Copiar padrão para
este jogo** recupera o `default.ini` personalizado. Gravações são atômicas; se
falharem, o ajuste anterior é mantido. Desfazer e pontos de retorno valem somente
na sessão e no perfil atual. Pausar gyro não pausa o jogo.

O painel completo tem assistente por jogo, presets iniciais e sugestões por sintoma.
Não são presets certificados para jogos específicos. Na TV, a interface nativa
oferece ajuda contextual junto dos ajustes.

## Situação atual

| Parte | Estado |
|---|---|
| Núcleo e perfis | Implementados, testes no Linux |
| Painel web/celular | Implementado; servidor pode rodar no PS5 |
| App gráfico nativo e serviço etaHEN | Compilação e integridade dos artefatos verificadas |
| Comunicação app ↔ serviço | Testada no host com o mesmo cliente C |
| Instalação/execução no PS5 13.60 | Pendente do console |
| Aplicar gyro nos jogos PS5 | Backend pendente; não há promessa de suporte universal |
| Overlay interativo sobre jogos | Não implementado |

## Código

```
src/core/    núcleo, perfis, HTTP e protocolo local versionado
src/native/  interface gráfica do app PS5 e cliente do serviço
src/ps5/     serviço, entrada de aplicação nativa e compatibilidade do runtime
src/linux/   bancada evdev/uinput e treino
tools/       build, dependências fixadas e ferramentas auxiliares de laboratório
tests/       testes portáteis e integração local
```

O projeto usa ferramentas e bibliotecas públicas; referências e versões em
[docs/THIRD_PARTY.md](docs/THIRD_PARTY.md). Nenhum arquivo proprietário de jogo
ou firmware é necessário para gerar a pasta desta build.
