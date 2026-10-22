# Diferenças conhecidas em relação ao original

"Original" aqui significa duas coisas diferentes:

- **GLydian** (o `.exe` fornecido): editor de patches. A reconstrução reproduz todas as
  funções dele e corrige os bugs encontrados.
- **TC Electronic G•Major** (o hardware que o GLydian controla): processador de efeitos. O
  DSP dele não está no material fornecido. A reconstrução implementa o DSP a partir do manual.

> **Não há equivalência sonora comprovada.** Nenhuma medição foi feita contra um G•Major
> real. Use `gmr_render --compare` com gravações do hardware para quantificar diferenças.

## 1. Em relação ao GLydian (editor)

| Item | GLydian | G-Major Reborn |
|---|---|---|
| Processamento de áudio | nenhum | engine DSP completo |
| Inteiros negativos ≤ −128 e −1 | decodificados errado | corretos (24 bits, complemento de 2) |
| Bits dos relés | lê 9/10, escreve 10/11 | lê e escreve 9/10 (**não verificado no hardware**) |
| Preset de usuário 27 | gera `lsb = −1` | numeração absoluta correta |
| Botão U (Undo) | stub (só imprime) | undo real por bloco, 64 níveis |
| Botão Get Lib | sem ação | carrega pasta de `.syx` |
| Curva dos modificadores | só armazena Min/Mid/Max | armazena **e aplica** (curva quadrática por 3 pontos) |
| Fontes M1–M4 | o pedal/CC é tratado pelo hardware | 4 parâmetros automatizáveis + MIDI CC com learn |
| Bytes não lidos do preset | descartados na regravação (o GLydian parte do SysEx original, como aqui) | preservados byte a byte |
| Relés | só gravados no preset | gravados + opcionalmente enviados como CC 80/81 |
| Edição Ping Pong Width | 0–100 | 0–100 (o manual diz 0–10; usamos a codificação do GLydian) |
| Faixa dos OUT LEVEL / INPUT / GLOBAL OUT | −100…0 dB | **−50…+50 dB** (pedido do usuário, v0.9.1); acima de 0 dB vai para o hardware limitado a 0 dB e, em arquivos, numa mensagem de extensão `F0 7D …` |
| Nível "Off" (−100 dB) vindo do hardware | mudo | carregado como −50 dB (mínimo da nova faixa); um modificador ainda pode levar a mudo |
| Plugins de um bloco só | não existem | 8 "Racks" VST3 (v0.9.1) |
| Comp. Threshold | faixa −40…+40 na classe | −40…0 dB (valores > 0 dBFS não fazem sentido em ponto flutuante normalizado) |

## 2. Em relação ao hardware G•Major (DSP)

| Bloco | Modelo implementado | Diferença esperada |
|---|---|---|
| Entrada/saída | ponto flutuante 32 bits, sem AD/DA | sem ruído, sem coloração de conversores, sem *Output Range* analógico |
| Noise Gate | ganho em dB com taxas constantes; Soft = expansor 1:3 | constantes Hard/Soft **inferidas** (1000 / 120 dB/s) |
| EQ | biquad RBJ peaking com largura em oitavas | forma exata da curva TC desconhecida |
| Compressor | feed-forward, detector de pico estéreo, knee de 4 dB | detector e knee da TC desconhecidos |
| Auto Resonance | envelope → passa-baixas ressonante 200 Hz…Freq Max | tempos Slow/Medium/Fast e escala de Sensitivity **inferidos**; o GLydian expõe 3 respostas, o manual cita 2 |
| Resonance Filter | SVF 12/24 dB/oct, Q até ≈12 | curva Q × Resonance inferida |
| Phasers | 4 e 12 all-pass de 1ª ordem (manual), faixas Low 100–1600 Hz / High 400–6400 Hz | faixas de varredura inferidas |
| Tremolo | Soft = cosseno elevado, Hard = quadrada suavizada (1,5 ms), Hi Cut na saída | posição exata do Hi Cut no original é ambígua no manual |
| Panner | pan de potência constante da soma mono | — |
| Chorus/Flanger | linha de atraso modulada (Hermite), Golden Ratio = excursão ∝ 1/velocidade | atrasos-base dos algoritmos "Classic" e a lei exata do Golden Ratio são inferidos |
| Pitch | shifter de 2 cabeças com janelas Hann | o G•Major tem detecção mais sofisticada; aqui pode haver leve *warble* em acordes e latência de janela (12–30 ms) |
| Whammy | "Pitch" = posição do pedal (glide até ±1/2 oitava, só sinal processado) | o manual descreve Pitch como mix seco/processado; implementamos glide, que é o uso típico com pedal |
| Delay | linhas Hermite, filtros de 1ª ordem no feedback, tempo suavizado (efeito "fita" ao mudar) | máximo 1800 ms; tempos sincronizados acima disso são limitados |
| Dynamic Delay | ducking com ataque de 10 ms e Release da tabela | ataque inferido |
| Reverbs | FDN 8 linhas Hadamard + 12 reflexões iniciais + difusores; Spring com cadeia dispersiva | são algoritmos próprios, **soam diferentes** dos da TC; Size/Color/Shape seguem a descrição do manual |
| Parallel/Semi-parallel | entrada passa uma vez; cada bloco soma o *wet* × Mix × Out | "Mix" age como nível de envio nos grupos paralelos (comportamento de loop de mixer, manual p.19) |
| Kill Dry | remove o sinal seco de todos os blocos com Mix | igual ao manual |
| Spill-over / FX Mute Soft | não implementado | ao trocar de preset as caudas são cortadas (rampa de 15 ms) |
| Tuner, MIDI Mapping, utilitários | não implementados | funções globais da unidade, fora do preset |
| Taxas | qualquer taxa (testado 44,1/48/96 kHz) | o hardware só opera a 44,1/48 kHz |
