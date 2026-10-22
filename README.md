# G-Major Reborn

Reconstrução independente, em C++17/JUCE, do processador de efeitos de guitarra
**TC Electronic G•Major** e do seu editor **GLydian**. Sai como plugin **VST3** e como aplicativo
**Standalone** para Windows 11 x64.

![Editor](docs/screenshots/editor_02.png)

- **Processamento de áudio real**: Noise Gate, EQ de 3 bandas, Compressor, Filter/Mod (Auto
  Resonance, Resonance, Vintage/Smooth Phaser, Tremolo, Panner), Pitch (Detune, Whammy,
  Octaver, Pitch Shifter), Chorus/Flanger (Classic/Advanced Chorus e Flanger, Vibrato), Delay
  (Ping Pong, Dynamic, Dual) e Reverb (Spring, Hall, Room, Plate). Roteamento Serial,
  Semi Parallel e Parallel, Kill Dry, tap tempo e sync com o host.
- **Presets byte-compatíveis com o hardware** (`.syx` de 615 bytes). O formato foi recuperado
  por engenharia reversa do GLydian.
- **Comunicação com um G•Major real** por MIDI: Tx/Rx do edit buffer e de presets, Tx RealTime,
  backup dos 100 presets de usuário.
- **Modificadores M1–M4** (pedal de expressão / MIDI CC / automação) com curva Min/Mid/Max,
  como no hardware.
- **Tudo automatizável**: 122 parâmetros. Nenhum controle é decorativo.
- **Racks individuais** (v0.9.1): 8 VST3 de um bloco só (Noise Gate, EQ, Compressor, Filter/Mod,
  Pitch, Chorus/Flange, Delay, Reverb), para usar em estágios diferentes da cadeia.
- **Volumes de −50 a +50 dB** (v0.9.1) em OUT LEVEL, INPUT e GLOBAL OUT.

![Racks](docs/screenshots/racks_overview.png)

> O DSP foi escrito a partir do manual e **não** foi comparado com um G•Major real. Veja
> [`KNOWN_DIFFERENCES.md`](KNOWN_DIFFERENCES.md). Projeto não afiliado à TC Electronic.

## Download

Baixe a versão mais recente na página **Releases** deste repositório:
- `GMajorReborn-<versão>-Setup-x64.exe`: instalador para Windows 10/11 64 bits (plugin
  completo, 8 racks, aplicativo e presets);
- `GMajorReborn-win64-portable.zip`: os mesmos arquivos avulsos, sem instalar.

Cada envio ao repositório é compilado e testado automaticamente (aba **Actions**), inclusive
num Windows real com o Visual Studio 2022.

## Documentos

| Documento | Conteúdo |
|---|---|
| [`CHANGELOG.md`](CHANGELOG.md) | o que mudou em cada versão |
| [`RECONSTRUCTION_REPORT.md`](RECONSTRUCTION_REPORT.md) | relatório da engenharia reversa e da reconstrução (evidências, arquitetura, testes) |
| [`INSTALL.md`](INSTALL.md) | instalação no Windows, compilação (MSVC, cross-compile, ASIO), uso |
| [`KNOWN_DIFFERENCES.md`](KNOWN_DIFFERENCES.md) | diferenças conhecidas em relação ao GLydian e ao hardware |
| [`docs/SYSEX_FORMAT.md`](docs/SYSEX_FORMAT.md) | mapa completo do preset SysEx (gerado do código) |
| [`docs/DSP_NOTES.md`](docs/DSP_NOTES.md) | modelagem de cada algoritmo e constantes inferidas |

## Build rápido

```bat
:: Windows, Visual Studio 2022
scripts\build-windows-msvc.bat
```
```bash
# só o core + testes, qualquer SO
scripts/build-linux-tests.sh
```

## Licença

GPL-3.0-or-later (o JUCE é usado sob a licença GPLv3). Ver [`LICENSE.md`](LICENSE.md).
"G•Major" e "TC Electronic" são marcas dos respectivos donos e aparecem aqui só para
identificar o equipamento compatível.
