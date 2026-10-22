# Avisos de componentes de terceiros

Este projeto utiliza ferramentas e componentes de terceiros. Consulte sempre os arquivos de licença distribuídos com cada dependência e a versão exata utilizada no build.

| Componente | Uso no projeto | Licença indicada |
|---|---|---|
| JUCE 7.0.12 | Framework do plugin VST3 e aplicativo Standalone | O build do projeto é publicado sob GPL-3.0-or-later; JUCE 7 também possui opções de licença próprias. Consulte o [EULA oficial do JUCE 7](https://juce.com/legal/juce-7-license/). |
| Módulos JUCE `juce_core`, `juce_audio_devices`, `juce_audio_basics`, `juce_events` | Módulos do JUCE | ISC, segundo o EULA do JUCE 7. |
| Steinberg ASIO SDK | Opcional para habilitar ASIO no Standalone | Licença Steinberg; não é incluído no repositório. |
| llvm-mingw | Toolchain opcional de cross-compilação | Apache-2.0 com exceções LLVM e licenças indicadas pelo projeto llvm-mingw; usado como ferramenta de build. |

O JUCE é obtido pelo CMake durante a configuração do plugin. O ASIO SDK deve ser obtido separadamente pelo usuário. Este resumo não substitui os textos das licenças dos respectivos componentes.
