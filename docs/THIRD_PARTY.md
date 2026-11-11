# Dependências públicas da build nativa

As dependências são baixadas fora do código do projeto, em `.deps/`. Versões e
SHA-256 estão em `tools/ps5-native-lock.json`. Os binários gerados são homebrew
experimental, não artefatos de um SDK proprietário da Sony.

- [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk), v0.45 usada na verificação:
  compilador/wrappers, headers e stubs públicos. GPL-3.0-or-later e licenças BSD dos headers.
- [ps5-app-sdk](https://github.com/cybervisi0n/ps5-app-sdk), commit
  `aee48ec8ec587a92c4ca31fe66667894e03ea9a7`: ferramentas de conversão para aplicação,
  contêiner FSELF, runtime aberto e stubs AGC. GPL-3.0; componentes de
  BlackBearReloaded indicados como GPL-3.0-or-later nos fontes.
- [ps5-sdl2](https://github.com/cybervisi0n/ps5-sdl2), v0.0.1:
  SDL2 e backend PS5. SDL tem licença zlib; o driver G19 incluído indica GPL-3.0-or-later.
- [ps5-mesa](https://github.com/cybervisi0n/ps5-mesa), 26.2.0-2;
  [ps5-opengnm-psbc](https://github.com/cybervisi0n/ps5-opengnm-psbc), v0.0.2;
  [ps5-gl-runtime](https://github.com/cybervisi0n/ps5-gl-runtime), v0.0.1:
  implementação gráfica pública e compilador usados pelo backend SDL. Consultar
  os respectivos fontes e avisos para as licenças de cada componente.
- [etaHEN-Plugins](https://github.com/etaHEN/etaHEN-Plugins): referência do formato
  de `.plugin` e das assinaturas da consulta de Title ID/notificação.

`src/ps5/app_start.c` é uma adaptação em C da convenção de entrada de
`ps5-app-sdk/crt/app_crt.cpp` (BlackBearReloaded, GPL-3.0-or-later), com atribuição
no próprio arquivo. O aplicativo e o runtime vinculam componentes GPL; uma
redistribuição deve preservar as licenças e disponibilizar os fontes correspondentes.
Os repositórios acima e o lock identificam os fontes usados na build local.

Nenhum `libc.prx` extraído de jogo/firmware, chave da Sony, imagem de jogo ou arquivo
oficial de distribuição acompanha esta build. O ícone é gerado geometricamente
pelo próprio projeto.
