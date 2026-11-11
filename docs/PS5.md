# Aplicativo homebrew nativo do PS5

O alvo do projeto é instalar **Gyro Aim no PS5**, com o DualSense conectado ao
console. O Linux serve para compilar, exercitar o núcleo e testar a interface
antes de o console estar disponível. Não há PC/Remote Play na arquitetura final.

## O que foi construído

### Aplicativo de configuração

`src/native/main.c` contém a interface gráfica, compilada para o PS5 e também
para o laboratório Linux. A build PS5 usa uma janela OpenGL nativa 1920×1080 e
renderização da interface em 1280×720, com navegação por DualSense. Não é uma página
web empacotada ou um executável Linux renomeado.

A interface possui abas de mira, sensor/recuperação, perfis e ajuda. Ajustes são
enviados ao serviço residente por localhost. O cliente não grava configurações
em uma segunda pasta. O app exibe o endereço do painel do celular fornecido pelo
serviço e informa quando a entrada nos jogos está indisponível.

Fechar o app encerra somente a interface. Ainda não foi validado no PS5 se abrir
ou alternar esse título preserva outro jogo em execução. O fluxo inicial é
configurar o perfil no app e depois abrir o jogo. Para ajustes durante o jogo,
o painel do celular é servido pelo console; atalhos e notificações dependerão
da integração do backend de entrada. Não há overlay interativo.

### Serviço residente do console

`src/ps5/main.c` é empacotado como `.plugin` do etaHEN com ID `GYRO00001`. Ele:

- Serve o painel e as APIs na porta 8090.
- Mantém perfis em `/data/gyroaim/profiles`.
- Consulta o Title ID com `sceSystemServiceGetAppIdOfRunningBigApp` e
  `sceSystemServiceGetAppTitleId`, com duas leituras estáveis antes de trocar.
- Ignora o Title ID do próprio app `PPSA94001` para permitir editar o último perfil.
- Descobre seu IPv4 e o fornece ao app como endereço do painel.
- Envia notificações comuns e fecha os sockets em SIGINT/SIGTERM.
- Não altera o ShellUI, banco de aplicativos, kernel ou memória dos jogos.

O serviço não lê/aplica gyro dentro de jogos nesta build. `input_ready` e
`output_ready` não são marcados artificialmente como ativos.

### Protocolo app ↔ serviço

As rotas `/api/native`, `/api/native/set`, `/api/native/action` e
`/api/native/title` e `/api/native/save_default` usam o mesmo estado e as mesmas operações do painel web.
O snapshot binário local tem magic, versão, tamanho, revisão, configurações,
perfil, mensagens e lista limitada de perfis. O cliente valida tamanho/versão,
strings e valores antes de usar a resposta.

Toda alteração exige Title ID e revisão atuais. Em conflito, a solicitação é
recusada; em falha de gravação, os valores anteriores são mantidos. A seleção
manual de perfil nativo é recusada quando o backend de saída informa que está
ativo, para não trocar a configuração sob um jogo que esteja recebendo entrada.
O servidor é para a rede local confiável, sem autenticação; não publique a porta
na internet.

## Compilar

```bash
make ps5-deps
make ps5 PS5_PAYLOAD_SDK=/caminho/ps5-payload-sdk
```

O primeiro comando baixa apenas dependências públicas fixadas em
`tools/ps5-native-lock.json`, verifica SHA-256 e extrai em `.deps/ps5-native`.
O segundo gera:

```
build/ps5/homebrew/PPSA94001/eboot.bin
build/ps5/homebrew/PPSA94001/sce_module/libc.prx
build/ps5/homebrew/PPSA94001/sce_sys/param.json
build/ps5/homebrew/PPSA94001/sce_sys/icon0.png
build/ps5/etaHEN/plugins/gyroaim-service.plugin
build/ps5/manifest.json
```

`eboot.bin` é um contêiner FSELF nativo com metadados SCE. `libc.prx` é o runtime
aberto gerado pelas ferramentas do ps5-app-sdk; não foi retirado de um jogo.
O serviço contém o cabeçalho de plugin do etaHEN e um ELF de payload, que é um
formato distinto do executável de aplicação.

O build inspeciona os contêineres e sua integridade. Isso verifica a geração dos
artefatos, não a aceitação pelo carregador do firmware 13.60. `manifest.json`
registra explicitamente `tested_on_console=false` e `game_input_hook=false`.

## Instalação a validar no console

A pasta de saída segue o formato homebrew documentado pelo
[ps5-app-sdk](https://github.com/cybervisi0n/ps5-app-sdk) e o formato de
[plugins do etaHEN](https://github.com/etaHEN/etaHEN-Plugins).

1. Copiar a pasta `homebrew/PPSA94001` para o local de homebrew aceito pelo
   carregador instalado, por exemplo `/data/homebrew/PPSA94001`.
2. Registrar a pasta no carregador compatível, como ShadowMount+, para mostrar o
   ícone **Gyro Aim** na tela inicial. Não executar o exemplo antigo de instalação
   que escreve em `/system_ex`.
3. Copiar `gyroaim-service.plugin` para `/data/etaHEN/plugins/` e iniciá-lo pela
   Toolbox do etaHEN. A política de inicialização automática do plugin pertence
   ao etaHEN já instalado.
4. Abrir Gyro Aim e verificar conexão com o serviço, navegação e gravação do perfil.
5. No celular, na mesma rede, abrir `http://IP-DO-PS5:8090`, mostrado na aba Ajuda.

Esses passos são o roteiro de validação, não uma instalação já testada. Não foi
alterado nem transmitido nada a um PS5 durante o desenvolvimento desta build.
A compatibilidade do carregador e da versão concreta do etaHEN precisa ser
confirmada no console disponível ao usuário.

## Entrada nos jogos: etapa ainda necessária

Ler o DualSense no app de configuração não altera o que outro processo recebe.
O serviço precisa de uma integração de entrada do console que aplique a conversão
nas leituras do jogo. Essa integração deve contemplar:

- `scePadReadState` e `scePadRead`, inclusive buffers de amostras.
- ABI PS4/PS5, unidade/eixos do sensor e timestamps reais.
- Identificação de jogo, controle e usuário; não modificar ShellUI ou processos de sistema.
- Configuração por perfil, supressão dos atalhos e notificações.
- Suspensão, retorno, desconexão, encerramento e retirada segura do hook.
- Jogos/engines com outras rotas de entrada e recusa explícita quando não suportados.

Não foi adicionado um hook com offsets adivinhados ou carregamento automático em
todos os jogos. A build atual não deve ser apresentada como gyro funcional nos
jogos PS5. O objetivo permanece completar essa integração nativa, não substituí-la
por Remote Play.

## Verificação já possível no Linux

```bash
make test test-http test-native
make native-lab
```

O mesmo cliente C da aplicação foi testado contra o serviço portátil real:
listar perfis, gravar, alternar jogos, rejeitar revisão antiga, restaurar fábrica,
desfazer e rejeitar snapshots incompatíveis. A interface pode ser renderizada
com SDL em modo offscreen sem ler o controle físico.
