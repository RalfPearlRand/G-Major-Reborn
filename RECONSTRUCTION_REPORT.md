# RECONSTRUCTION_REPORT — G-Major Reborn

Relatório de progresso da engenharia reversa e da reconstrução do software de guitarra
fornecido (`GLydian.exe` + `GMajor_Manual.pdf` + captura do painel).

Legenda de confiança usada em todo o documento:

| Marca | Significado |
|---|---|
| **[CONFIRMADO]** | lido no código decompilado / bytes do arquivo, com coordenada citada |
| **[MANUAL]** | descrito no manual do G•Major (página citada) |
| **[INFERIDO]** | dedução consistente com as evidências, sem verificação direta |
| **[DESCONHECIDO]** | não determinável sem o hardware real |

---

## 1. Resumo executivo

1. **O `GLydian.exe` não processa áudio.** É um *wrapper* Launch4j em torno de um JAR Java
   que implementa um **editor de patches MIDI/SysEx** para o processador de hardware
   **TC Electronic G•Major**. Todo o DSP original roda no hardware (DSP da TC), que não
   foi fornecido. **[CONFIRMADO]**
2. O que o executável contém — e foi recuperado com exatidão — é o **modelo de dados do
   equipamento**: layout do preset SysEx de 615 bytes, faixas e tabelas de valores de cada
   parâmetro, lista de algoritmos, slots de modificadores M1–M4 e o protocolo de pedido de
   preset. **[CONFIRMADO]**
3. A reconstrução (`G-Major Reborn`) implementa:
   - um **core C++17 independente** (sem JUCE) com o codec SysEx byte-compatível com o
     G•Major, 8 blocos de efeito (Gate, EQ, Compressor + 5 blocos com **22 algoritmos**) de DSP real, roteamento
     Serial/Semi-paralelo/Paralelo, Kill Dry, modificadores, tap tempo/host sync;
   - um **plugin VST3 + aplicativo Standalone (JUCE 7.0.12)** para Windows 11 x64, com GUI no
     layout do GLydian, onde todo controle tem função real;
   - **comunicação MIDI com o G•Major real** (Tx/Rx do edit buffer e de presets), presets em
     `.syx` intercambiáveis com o hardware;
   - **suítes de testes automatizados** (core: 32 testes, ≈276 mil verificações; integração
     plugin+GUI: ≈19,9 mil verificações) e ferramenta de render/A-B offline.
4. **Não há afirmação de equivalência sonora.** Os algoritmos de DSP são aproximações
   documentadas a partir do manual (seção 6 e `KNOWN_DIFFERENCES.md`). A ferramenta
   `gmr_render --compare` existe para medir a diferença contra gravações de um G•Major real.
5. O G•Major **não tem simulação de amplificador/distorção** **[MANUAL p.5, p.27-45]**.
   Nada disso foi inventado na reconstrução.

---

## 2. Fase 1 — Reconhecimento (inventário)

### 2.1 Arquivos fornecidos

| Arquivo | Tamanho | Identificação |
|---|---|---|
| `GLydian.exe` | 735 373 B | PE32 (x86), GUI, timestamp 2007-03-23; SHA-256 `b9d45168183ebff0503f1a1003cca5848bae880919eacabdb5c7adefa532ee9a` |
| `GMajor_Manual.pdf` | 1 049 300 B, 56 páginas | Manual TC Electronic G•Major, Rev 7.1, SW V1.26 |
| `image.png` | 285 344 B | Captura da janela "GLydian (G-MAJOR GUITAR PROCESSOR EDITOR)" |

Não havia DLLs, presets, bancos de dados nem arquivos de configuração no diretório.

### 2.2 Análise estática do executável **[CONFIRMADO]**

- Seções PE: `.text` 15 KB, `.rsrc` 288 KB (entropia 7,4), sem `.NET`.
- String `Launch4j` no offset de arquivo **16400**; URL `http://java.com/download` (stub
  que procura uma JRE).
- **Overlay ZIP** (assinatura `PK\x03\x04`) no offset **308224** até o fim do arquivo →
  JAR com **343 entradas**, `META-INF/MANIFEST.MF` → `Main-Class: GLydian`.
- Classes: `com.glydian.model.*` (modelo/MIDI), `com.glydian.model.effect.*` (efeitos),
  `com.glydian.gui.*` (Swing). Bytecode versão 49 (Java 5, com assinaturas Java 6).
- Decompilação: CFR 0.152 → 34 arquivos-fonte (≈12,4 mil linhas). Desmontagem
  complementar com `javap -c -p`.
- **Nenhuma** classe de áudio (`javax.sound.sampled`), nenhum DSP. Usa apenas
  `javax.sound.midi` (`SysexMessage`, `MidiSystem`, `Receiver/Transmitter`).
- Nenhum mecanismo de licença/DRM encontrado (e nenhum foi contornado).
- O binário **não foi executado** em nenhum momento.

### 2.3 Mapa de componentes do GLydian

| Componente (classe) | Função | Coordenada |
|---|---|---|
| `GmajorPatch` | modelo do preset; parse/geração do SysEx de 615 B; checksum | `GmajorPatch.initialize`, `createSysex` |
| `BitmaskField`, `BitInputStream` | campos de bits empacotados 7 bits/byte, LSB primeiro | `readBitsfromByteArray` |
| `ModifireField` | slot de modificador (fonte M1–M4, Min/Mid/Max) | `initialize`, `writeToSysex` |
| `DiscretRange` + subclasses | tabelas de valores (Speed, HiCut, EQ Freq, Ratio…) | `static String[]` |
| `effect.Gm*Effect` | offsets e faixas de cada bloco/algoritmo | `initialize(byte[])` |
| `GmMidiManager` | abertura de portas, pedido de preset `F0 00 20 1F 00 48 45 …` | `preparePatchRequestMsg` |
| `GmMidiCommManager` | fila de transmissão em tempo real ("Tx RealTime") | `MidiCommQueue` |
| `PatchLibrary` | biblioteca de patches em memória (nome → patch) | `add`, `getCopyableEffects` |
| `gui.GLydianSwing` | janela, menus, botões Tx/Rx/Disk/Lib | `MainDisplay` |
| `gui.EfectComposit`, `MultiAlgorithmComposit` | painéis dos blocos, botões U/E, toggles de modificador | `ModifireButton`, `EditEffectMenu` |

---

## 3. Fase 2 — Engenharia reversa

### 3.1 Fluxo de sinal **[MANUAL p.9, p.21]**

```
Input → [Input level] → Noise Gate → (EQ) → Compressor → Filter/Mod →
   Serial:        Pitch → Cho/Fla → Delay → Reverb
   Semi Parallel: Pitch → Cho/Fla → ( Delay ‖ Reverb )
   Parallel:      ( Pitch ‖ Cho/Fla ‖ Delay ‖ Reverb )
→ [Preset Out Level] → [Global Out] → Bypass → Output
```
A posição do EQ não aparece nos diagramas (o EQ foi acrescentado num firmware posterior e
compartilha a tecla do Noise Gate **[MANUAL p.28]**); foi colocado logo após o gate **[INFERIDO]**.

### 3.2 Formato do preset SysEx **[CONFIRMADO]** (detalhes em `docs/SYSEX_FORMAT.md`)

- 615 bytes: `F0 00 20 1F 00 48 20 lsb msb <nome 20 B> <32 slots de modificador> <bitfield> <parâmetros> <checksum> F7`.
- Checksum = soma dos bytes 28..611 (coordenadas de `getData()`) `& 0x7F`.
- Inteiros: 4 bytes, **complemento de 2 de 24 bits empacotado 7 bits/byte, LSB primeiro**
  (o 4º byte guarda os bits 21–23; `07` ⇒ negativo).
- Número do preset = `lsb + 128·msb`: 0 = edit buffer, 1–100 factory, 101–200 user
  **[INFERIDO, consistente com `createSysex` e `initialize`]**.
- Pedido de preset: `F0 00 20 1F 00 48 45 lsb msb F7` (`GmMidiManager.presetRequestMessageHead`).
- On/off dos blocos: 0 = ligado (exceto EQ: 1 = ligado) — `GmEffect.setEffectOnOffFlag`, `GmEqualizer`.
- Tabelas: armazenado = índice + deslocamento (3 para tempos e larguras do EQ, 25 para a frequência do EQ).

### 3.3 Bugs encontrados no GLydian (documentados, não reproduzidos)

| # | Bug | Evidência | Tratamento na reconstrução |
|---|---|---|---|
| 1 | `readInt` decodifica errado negativos ≤ −128 (ex.: Offset −200, Voice −2400) | `GmajorPatch.readInt`; teste `packed24_roundtrip_and_glydian_bug` | codec em 24 bits complemento de 2 correto |
| 2 | `readInt` decodifica **−1 como 0** (`7F 7F 7F 07`) | idem | idem |
| 3 | Relés **lidos** nos bits 9/10, mas **escritos** nos bits 10/11 | `initialize` vs `createSysex` | leitura e escrita nos bits 9/10; **[DESCONHECIDO]** qual é o do hardware |
| 4 | Preset de usuário nº 27 gera `lsb = −1` (fronteira `<27`/`>27`) | `createSysex` | numeração absoluta `lsb + 128·msb` |
| 5 | Botão **U** (Undo) só imprime "Undo edit ..." no console | `EfectComposit` linha ~158 | Undo real por bloco (64 níveis) |
| 6 | Botão **Get Lib** não tem ação associada | `GLydianSwing.MainDisplay` (`openFromLibBtn`) | carrega uma pasta de `.syx` como biblioteca |
| 7 | `pitchEft.writeToSysex` chamado duas vezes | `createSysex` | inofensivo; não reproduzido |

### 3.4 Semântica dos botões **[CONFIRMADO]** (`GLydianSwing.MainDisplay`)

| Botão | GLydian | G-Major Reborn |
|---|---|---|
| Tx Playing | envia o patch como "externo" (número 0 = edit buffer) | idem |
| Rx Playing | pede o preset 0 (edit buffer) | idem |
| Tx Bank | envia o patch com o banco/número escolhido (FACTORY → 0) | idem |
| Rx Bank | pede o preset do banco/número escolhido | idem (a resposta vira o patch atual) |
| Save/Open Disk | `.syx` de 615 B | idem (Open lê arquivos com vários presets) |
| Add Lib | adiciona à biblioteca (mesmo nome substitui) | idem |
| Get Lib | sem ação | carrega pasta de `.syx` |
| U / E | Undo (stub) / copiar bloco de um patch da biblioteca com o bloco ativo | Undo real / idem |
| OFF (sob os knobs) | atribui modificador OFF/M1–M4 | idem + editor da curva Min/Mid/Max |
| Tx RealTime | reenvia o patch a cada edição | idem (com debounce de ~170 ms) |

### 3.5 O que continua desconhecido **[DESCONHECIDO]**

- Os algoritmos DSP reais da TC (filtros, pitch, reverbs): nenhum código deles existe no
  material fornecido.
- Os bytes 184–223, 604–607 e alguns outros do preset não são lidos pelo GLydian; a
  reconstrução os **preserva byte a byte** em ida e volta.
- A semântica exata dos modificadores no hardware (o GLydian só guarda os campos). A
  reconstrução aplica `valor = mín + curva(Min,Mid,Max)·(máx − mín)` **[INFERIDO]**.
- Polaridade/bit dos relés (bug 3).

---

## 4. Fase 3 — Arquitetura da reconstrução

```
gmajor-reborn/
├─ core/                    C++17 puro, sem dependências (testável headless)
│  ├─ include/gmr/Tables.h      tabelas de valores (geradas do GLydian: tools/gen_tables.py)
│  ├─ include/gmr/Params.h      tabela única de 122 parâmetros (113 do preset + 9 da unidade): id, faixa, offset SysEx, algoritmos
│  ├─ include/gmr/Patch.h       modelo de preset + codec SysEx 615 B (+ preservação de bytes)
│  ├─ include/gmr/Engine.h      engine em tempo real (roteamento, rampas, modificadores, tempo)
│  ├─ include/gmr/dsp/*.h       blocos: Gate, EQ, Comp, FilterMod, Pitch, ChorusFlanger, Delay, Reverb
│  └─ src/…                     implementação + FactoryPresets (20 presets originais do projeto)
├─ plugin/                  JUCE 7.0.12: VST3 + Standalone
│  └─ Source/               Processor (APVTS, estado, MIDI), Editor, Sections, Controls, PresetLibrary
├─ tests/                   TestMain.cpp (core) + PluginHarness.cpp (processor + GUI headless)
├─ tools/                   gmr_render (render offline, A/B, dump, mapa SysEx), gen_tables.py
├─ cmake/                   toolchain llvm-mingw (cross Linux→Windows) + stubs X11 só do host
├─ presets/                 20 presets .syx (formato do hardware)
└─ docs/                    SYSEX_FORMAT.md, DSP_NOTES.md, screenshots
```

**Decisões de arquitetura**

- **Domínio "armazenado"**: todo parâmetro é guardado como o mesmo inteiro que o hardware
  guarda (índice de tabela, dB, ms…). O mesmo valor serve ao codec, ao DSP e à automação, e
  isso elimina erros de conversão. Modificadores geram valores fracionários entre passos, e o
  DSP interpola as tabelas.
- **Thread-safety**: GUI/host → `std::atomic<int>` por parâmetro; o áudio lê um *snapshot* a
  cada sub-bloco de 32 amostras. SysEx/CC da thread de áudio → FIFO lock-free → timer da
  message thread. **Nenhuma alocação, lock ou I/O no callback** (buffers alocados em
  `prepare()`; `MidiBuffer` pré-reservado).
- **Clique-zero**: rampas de 15 ms para liga/desliga de blocos, troca de algoritmo (reset +
  fade-in), troca de roteamento (mergulho curto) e bypass.
- **Proteção**: qualquer NaN/Inf na saída zera o bloco, reseta o DSP e é contado
  (`resetsAfterNonFinite`).
- **Latência**: 0 amostras reportadas (sem look-ahead). Os blocos de pitch têm latência
  intrínseca de janela (~12–30 ms, como qualquer pitch shifter em domínio do tempo).

---

## 5. Fase 4 — Fidelidade: matriz de funcionalidades

Todos os 113 parâmetros do preset + 9 de "unidade" (122 no total) são **automatizáveis** no host e têm
efeito real no áudio ou na comunicação MIDI.

| Bloco / recurso | Algoritmos / controles | Estado |
|---|---|---|
| Noise Gate | Soft/Hard, Threshold, Max Damp, Release (dB/s) | implementado (modelo próprio) |
| EQ | 3 bandas paramétricas, Freq 40,97 Hz–20 kHz/Off, ±12 dB, 0,2–4 oct | implementado (RBJ peaking) |
| Compressor | Threshold, Ratio Off…∞:1, Attack, Release, Gain | implementado (feed-forward, knee 4 dB) |
| Filter/Mod | Auto Resonance, Resonance (2ª/4ª ordem), Vintage Phaser (4 AP), Smooth Phaser (12 AP), Tremolo (Soft/Hard, Pulse Width, Hi Cut), Panner | implementado |
| Pitch | Detune (2 vozes ±100 c, delays), Whammy (pedal via M1–M4), Octaver (±1/2 oct), Pitch Shifter (2 vozes ±2400 c, pan, delay 0–350 ms, feedback, level) | implementado (shifter de 2 cabeças) |
| Chorus/Flanger | Classic/Advanced Chorus (Golden Ratio, Phase Reverse, Delay), Classic/Advanced Flanger (FB ±100, FB Hi Cut), Vibrato | implementado |
| Delay | Ping Pong (Width), Dynamic (Offset R, Sensitivity, Damping, Release — ducking), Dual (2 linhas, tempo, FB, pan) + FB Hi/Lo Cut | implementado |
| Reverb | Spring, Hall, Room, Plate; Decay, Pre Delay, Shape, Size, Hi/Lo Color+Factor, Room/Reverb Level, Diffuse | implementado (FDN 8 linhas + ER) |
| Roteamento | Serial, Semi Parallel, Parallel | implementado |
| Níveis | Preset Out Level, Input, Global Out, Kill Dry, Bypass | implementado |
| Tempo | Tap Tempo (botão TAP), subdivisões 1 bar…1/32T, sync com host | implementado |
| Modificadores | 32 slots / 95 atribuições por algoritmo, M1–M4 com curva Min/Mid/Max; fontes = 4 parâmetros automatizáveis + MIDI CC (learn) | implementado |
| Relés 1/2 | guardados no preset, enviados ao hardware; opcional: CC 80/81 na saída MIDI | implementado (não há relé físico num PC) |
| MIDI hardware | Tx/Rx Playing, Tx/Rx Bank, Tx RealTime, receber/enviar 100 presets de usuário, Program Change | implementado, **não testado com hardware** |
| Presets | `.syx` byte-compatíveis; biblioteca em pasta/arquivo de banco; estado no projeto do host (inclui biblioteca) | implementado |
| GUI | layout do GLydian, U (undo real) e E (copiar bloco), medidores IN/OUT/GATE/COMP, status MIDI | implementado |
| Áudio standalone | WASAPI, DirectSound; ASIO com o SDK Steinberg (opção de build) | implementado (ASIO depende do SDK) |
| Taxas | 44,1 / 48 / 96 kHz (qualquer taxa funciona; coeficientes dependem de `fs`) | testado |

---

## 6. Diferenças em relação ao original

Ver `KNOWN_DIFFERENCES.md` (lista completa). Resumo:
- DSP é **reconstrução a partir do manual**, não do código da TC. Não foram feitas medições
  contra um G•Major real.
- O hardware tem conversão AD/DA, saídas balanceadas e relés físicos; o plugin não.
- Parâmetros globais do hardware (tuner, MIDI mapping, utilitários) estão fora do escopo do
  preset e só os relevantes ao áudio foram reproduzidos (Input, Global Out, Kill Dry, Bypass).

---

## 7. Fase 5 — Testes (todos passando)

| Suíte | Conteúdo | Resultado |
|---|---|---|
| `gmr_tests` (core) | tabelas = GLydian; consistência da tabela de parâmetros (sem colisão de offsets por algoritmo); codec 24 bits + bugs do GLydian; layout/cabeçalho/checksum; **fuzz de 2000 presets aleatórios** com ida-e-volta byte a byte; preservação de bytes desconhecidos; rejeição de lixo; arquivos `.syx` multi-preset; passagem bit-exata (44,1/48/96 kHz); bypass; ganhos/Off; headroom sem clipping interno; curva do compressor e limiter; resposta em frequência do EQ (+12 dB ± 0,3 a 1 kHz, 44,1 e 96 kHz); inclinação 12/24 dB/oct do filtro ressonante; tempo de eco a 3 taxas; tempo sincronizado (tap e host); Kill Dry; roteamento paralelo; decaimento dos 4 reverbs vs Decay; octaver e pitch shifter (análise espectral); Whammy via modificador; curva do modificador; noise gate; tremolo 100 %; **fuzz de estabilidade** (todos os blocos, presets aleatórios, 3 taxas); denormais; NaN contido e recuperação; **mudança aleatória de parâmetros a cada 256 amostras por 10 s**; independência do tamanho de buffer (1…4096); liga/desliga sem clique; presets de fábrica | **32/32 OK, 275 748 verificações** |
| `gmr_tests` sob ASan + UBSan | mesma suíte | **sem erros** |
| `gmr_plugin_harness` (Xvfb) | estado do host (APVTS + meta + modificadores + CC + biblioteca); SysEx entrando (edit buffer → patch, user → biblioteca); Tx Playing gera exatamente o SysEx esperado; Rx Playing/Rx Bank geram o pedido correto e carregam a resposta; Tx Bank grava no número certo; CC → M1 → Whammy; Program Change; MIDI learn; layouts mono→estéreo; blocos de 1…1024 amostras a 3 taxas; editor: todos os 22 algoritmos multi-bloco sem controles fora dos limites ou sobrepostos; capturas de tela | **19 873 verificações, 0 falhas** |

Desempenho medido (Linux, Xeon 2,8 GHz, 1 núcleo, os 8 blocos ligados com os algoritmos
mais caros, incluindo I/O de WAV): **48 kHz ≈ 8,8 % de um núcleo; 96 kHz ≈ 16,6 %**.

O harness também encontrou e corrigiu um bug real: um callback de parâmetro era disparado
antes da construção completa do componente (ordem de inicialização em `LedGroup`).

---

## 8. Fase 6 — Compilação

| Artefato | Plataforma | Como foi gerado |
|---|---|---|
| `G-Major Reborn.vst3` (bundle) | Windows x64 | cross-compile llvm-mingw 20250613 (clang 20.1.7), estático, sem DLLs extras |
| `G-Major Reborn.exe` (Standalone) | Windows x64 | idem |
| `gmr_render.exe`, `gmr_tests.exe` | Windows x64 | idem |
| `GMajorReborn-0.9.0-Setup-x64.exe` | Windows x64 | instalador Win32 próprio (`installer/`), payload embutido e verificado byte a byte; requer admin; registra desinstalação |

Dependências dos binários Windows: apenas DLLs do sistema + UCRT (nativos no Windows 10/11).
Build com **0 warnings** no código do projeto.

Os binários Windows **não puderam ser executados** neste ambiente (Linux sem Wine). Toda a
lógica (core, processor, editor) foi executada e testada no build Linux equivalente. O passo
de verificação restante é abrir o VST3/Standalone num Windows 11 real
(`INSTALL.md` → "Checklist de verificação").

---

## 9. Próximos passos recomendados

1. Validar Tx/Rx com um G•Major real (bit dos relés, semântica dos modificadores).
2. Gravar o hardware com sinais de teste (varredura, impulso, ruído rosa, DI de guitarra)
   para cada preset e rodar `gmr_render --compare` para ajustar os algoritmos.
3. Opcional: ASIO (baixar o SDK Steinberg e recompilar com `-DGMR_ASIO_SDK_DIR`).

## 9.1 Versão 0.9.1 (pedido do usuário)

Ver `CHANGELOG.md`. Em resumo: volumes de −50 a +50 dB, mantendo a compatibilidade com o
hardware (limitação no Tx e mensagem de extensão `F0 7D` nos arquivos); modificadores em OUT
LEVEL presos à faixa do hardware (pedal nunca ultrapassa 0 dB); **8 VST3 de bloco único**
("Racks"), gerados a partir do mesmo código com `GMR_RACK_BLOCK`.

Testes após a mudança: core **35/35 OK**; harness de integração com os racks
(parâmetros expostos, isolamento de áudio, presets, estado, layout de todos os algoritmos e
capturas) **76 478 verificações, 0 falhas**.

## 10. Registro de progresso

| Etapa | Situação |
|---|---|
| Inventário + triagem do PE | concluído |
| Extração do JAR, decompilação, mapa de componentes | concluído |
| Leitura do manual (efeitos, roteamento, níveis, MIDI, especificações) | concluído |
| Codec SysEx + tabela de parâmetros | concluído |
| DSP (8 blocos, 22 algoritmos multi-bloco + gate/EQ/comp) + engine | concluído |
| Testes core (+ sanitizers) | concluído |
| Plugin VST3/Standalone + GUI | concluído |
| Harness de integração + capturas da GUI | concluído |
| Build Windows x64 | concluído |
| v0.9.1: volumes ±50 dB + racks VST3 individuais | concluído |
| Teste em Windows real / com hardware | **pendente (requer o seu PC / o G•Major)** |
