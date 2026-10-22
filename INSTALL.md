# Instalação e compilação — G-Major Reborn

## 1. Instalar (Windows 11 x64)

### 1.1 Instalador (recomendado)

Execute **`GMajorReborn-0.9.1-Setup-x64.exe`** e aceite o pedido de administrador. Ele é
necessário para gravar em *Arquivos de Programas* e em *Arquivos Comuns\VST3*. Componentes
(escolha na tela):

| Componente | Destino |
|---|---|
| Plugin VST3 completo | `C:\Program Files\Common Files\VST3\G-Major Reborn.vst3` |
| Racks VST3 individuais (8) | `C:\Program Files\Common Files\VST3\G-Major <Bloco> Rack.vst3` |
| Aplicativo Standalone + ferramentas | `C:\Program Files\G-Major Reborn\` (pasta alterável) |
| Presets | `Documentos\G-Major Reborn\presets` (+ cópia na pasta do aplicativo) |
| Atalhos | Menu Iniciar (padrão) e Área de Trabalho (opcional) |

O instalador registra o programa em *Configurações → Aplicativos*, e é por lá que se
desinstala (ou pelo atalho "Desinstalar"). Reinstalar por cima atualiza a instalação na mesma
pasta. Modo silencioso: `GMajorReborn-0.9.1-Setup-x64.exe /S [/D=C:\pasta]`;
desinstalação silenciosa: `"C:\Program Files\G-Major Reborn\Uninstall.exe" /uninstall /S`.
O instalador não é assinado digitalmente, então o SmartScreen pode avisar
("Mais informações → Executar assim mesmo").

### 1.2 Manual (ZIP)

O pacote `GMajorReborn-win64` contém:

```
VST3/G-Major Reborn.vst3/            ← bundle VST3 (pasta)
Standalone/G-Major Reborn.exe        ← aplicativo independente
tools/gmr_render.exe                 ← render offline / comparação A-B / dump de .syx
tools/gmr_tests.exe                  ← suíte de testes do core (rode para validar a máquina)
presets/*.syx                        ← 20 presets (formato do G•Major)
```

**VST3:** copie a pasta inteira `G-Major Reborn.vst3` para
`C:\Program Files\Common Files\VST3\` e peça ao seu DAW para escanear plugins.
Não é preciso instalador, nenhuma DLL extra e nenhum driver (só DLLs do sistema e o UCRT, que
já vem no Windows 10/11).

**Standalone:** execute `G-Major Reborn.exe`. Os binários não são assinados, então o SmartScreen
pode avisar ("Mais informações → Executar assim mesmo"). Em *Options → Audio/MIDI Settings*:

- **Driver**: *Windows Audio* (WASAPI), *Windows Audio (Exclusive Mode)* para menor latência,
  *DirectSound* ou *ASIO* (só se compilado com o SDK ASIO; ver 3.3).
- **Sample rate**: 44100, 48000 ou 96000 Hz. **Buffer**: 64–256 amostras.
- **MIDI input / output**: escolha a interface ligada ao G•Major para usar Tx/Rx, ou o
  controlador/pedaleira para M1–M4 e Program Change.

### Checklist de verificação em Windows (recomendado)

1. `tools\gmr_tests.exe` → deve terminar com `0 failed tests`.
2. Abra o Standalone, selecione a interface e toque: com o preset *Clean Chorus Room* deve
   haver chorus + room.
3. Ligue/desligue blocos, troque algoritmos e roteamento durante a execução: sem cliques.
4. Abra o VST3 no DAW, automatize um parâmetro e salve/reabra o projeto (o estado e a
   biblioteca de patches voltam).
5. Com um G•Major ligado por MIDI: *Rx Playing* deve trazer o patch atual da unidade.

## 2. Uso rápido

- **Presets**: *File → Open .syx* aceita arquivos com 1 ou vários presets (backup do
  G•Major). *Patch Library* lista a biblioteca; o host vê cada entrada como "program".
- **Modificadores (pedal de expressão)**: clique em `OFF` sob um knob → escolha M1–M4 e a curva.
  M1–M4 são parâmetros automatizáveis e respondem a MIDI CC (padrão: M1 = CC 11,
  M2 = CC 1, M3 = CC 4, M4 = CC 2; clique com o botão direito no rótulo para *learn*).
- **U** desfaz edições do bloco; **E** copia o bloco de um patch da biblioteca.
- **Racks**: no DAW, procure por "G-Major … Rack". Cada um processa só aquele bloco. Use
  OUTPUT (−50…+50 dB) para compensar volume e KILL DRY para usar em envio auxiliar.
  "Open .syx" pega as configurações do bloco num preset do G•Major.
- **TAP** marca o tempo; *TEMPO SRC = Host* segue o andamento do DAW.

## 3. Compilar a partir do código

### 3.1 Windows com Visual Studio 2022 (recomendado para quem desenvolve)

Requisitos: Visual Studio 2022 (workload *Desktop development with C++*), CMake ≥ 3.22, Git.

```bat
scripts\build-windows-msvc.bat
```
ou manualmente:
```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DGMR_JUCE_DIR=C:\caminho\JUCE
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```
Sem `GMR_JUCE_DIR`, o CMake baixa o JUCE 7.0.12 do GitHub automaticamente.
Saída: `build\plugin\GMajorReborn_artefacts\Release\{VST3,Standalone}`.

### 3.2 Cross-compile Linux → Windows (como os binários deste pacote foram feitos)

```bash
export LLVM_MINGW_ROOT=/opt/llvm-mingw-20250613-ucrt-ubuntu-22.04-x86_64   # github.com/mstorsjo/llvm-mingw
export JUCE_DIR=/opt/JUCE-7.0.12                                          # JUCE 8 removeu o suporte a MinGW
scripts/build-windows-cross.sh
```
Se a máquina Linux não tiver `libxrandr-dev libxinerama-dev libxcursor-dev`, o script usa os
stubs de declaração em `cmake/host-x11-stubs/`, necessários só para a ferramenta `juceaide` do
JUCE, que roda no host durante o build.

### 3.3 Gerar o instalador

Depois do build cross (3.2):
```bash
LLVM_MINGW_ROOT=... BUILD_DIR=build-win scripts/make-installer.sh     # -> dist/GMajorReborn-<versão>-Setup-x64.exe
```
O instalador (`installer/Installer.cpp`) é um executável Win32 próprio. Os arquivos vão
embutidos como recursos, listados por `installer/gen_payload.py`, e o executável não depende de
NSIS, Inno Setup ou WiX.

### 3.4 ASIO

O driver ASIO exige o **Steinberg ASIO SDK** (download em steinberg.net, licença própria da
Steinberg). Baixe e descompacte, depois:
```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DGMR_ASIO_SDK_DIR=C:\caminho\asiosdk
```
O log do CMake confirma `ASIO enabled`. O VST3 não precisa de ASIO: o DAW cuida do driver.

### 3.5 Só o core + testes (qualquer SO, sem JUCE)

```bash
cmake -S . -B build -DGMR_BUILD_PLUGIN=OFF && cmake --build build && ctest --test-dir build
```

### 3.6 Harness de integração do plugin (Linux)

```bash
cmake -S . -B build-h -DGMR_BUILD_HARNESS=ON -DGMR_JUCE_DIR=/opt/JUCE-7.0.12
cmake --build build-h --target gmr_plugin_harness
xvfb-run -a build-h/plugin/gmr_plugin_harness_artefacts/Release/gmr_plugin_harness shots/
```

## 4. Ferramenta de linha de comando

```bash
gmr_render in.wav out.wav --preset "presets/02 Lead Delay Hall.syx"     # processa um WAV
gmr_render in.wav out.wav --set dly_on=1 --set dly_time=375             # parâmetros por chave
gmr_render --compare gravacao_gmajor.wav out.wav                        # A/B com o hardware
gmr_render --dump backup_do_gmajor.syx 3                                # lista o 4º preset do arquivo
gmr_render --sysex-map                                                  # tabela do formato
```
As chaves de parâmetro estão em `docs/SYSEX_FORMAT.md` (coluna *Key*).
