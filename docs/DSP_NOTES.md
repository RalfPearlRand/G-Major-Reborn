# Notas de DSP

Todos os blocos estão em `core/src/dsp/`. Cada um transforma o buffer no sinal **100 % wet**.
O `Engine` aplica Mix, Out Level, Kill Dry, rampas e roteamento, de modo uniforme.

## Convenções

- Valores chegam no domínio armazenado (o mesmo inteiro do preset), via `Ctx::val()`, que
  converte para unidades físicas e interpola tabelas quando há modificadores (valores
  fracionários).
- `Tempo` ≠ "Ignored" substitui `Speed`/`Time`: duração = `beat × 4 × fração` (1 = 1 compasso,
  1/4D = 3/8 de compasso…). `beat` = Tap Tempo (ms) ou o andamento do host
  (`TEMPO SRC = Host`).
- Níveis −100 dB = Off (ganho 0).
- Atualização de parâmetros a cada 32 amostras; suavização interna nos parâmetros sensíveis
  (cutoff do auto-wah, tempos de delay, razões de pitch, atrasos de chorus).

## Mix e roteamento

```
bloco serial:      y = out · ( (1−mix)·seco·[!killDry] + mix·wet )      (blocos sem Mix: y = out·wet)
grupo paralelo:    y = seco·[!killDry] + Σ out_i · mix_i · wet_i
liga/desliga:      y' = seco + (y − seco)·g,   g = rampa 0↔1 em 15 ms
```

## Por bloco

| Bloco | Implementação | Constantes inferidas |
|---|---|---|
| Noise Gate | detector de pico (0,1 ms / 40 ms), histerese de 3 dB, hold de 15 ms; ganho em dB com taxa de abertura = Release (dB/s) | Hard fecha a 1000 dB/s até −MaxDamp; Soft = expansor 1:3 a 120 dB/s |
| EQ | 3 biquads peaking (RBJ, BW em oitavas); banda com Freq "Off" ou 0 dB é ignorada | — |
| Compressor | ganho = −(1−1/R)·excesso, knee de 4 dB; balística em dB (ataque quando aumenta a redução) | knee |
| Auto Resonance | envelope (L+R)/2 × 2^Sens × 0,08 → cutoff 200 Hz…FreqMax (exp.), SVF LP Q 4,5 (2ª) ou 2× Q 2,2 (4ª) | tempos 25/450, 8/200, 2/70 ms |
| Resonance | SVF LP; Q = 0,707 + res²·11,3; 4ª ordem = estágio Q 0,54 + estágio ressonante | — |
| Phaser | N all-pass de 1ª ordem (4 Vintage, 12 Smooth), LFO senoidal exponencial na faixa, feedback ±0,85, Phase Reverse = LFO direito +180° | faixas Low/High |
| Tremolo | fase deformada pelo Pulse Width; Soft = cosseno, Hard = quadrada; ganho = 1 − Depth·(1−u), suavizado 1,5 ms; Hi Cut (1 polo) | — |
| Panner | pan de potência constante da soma mono; Width = profundidade | — |
| Chorus/Flanger | atraso = base + exc·(½+½ sen); Classic Chorus 7 ms / exc 6 ms, Classic Flange 0,4 ms / 4,5 ms, Vibrato 0,5 / 5 ms; Advanced usa Delay (0,1–50 ms); Golden Ratio: exc ×= clamp(0,6 Hz / rate, 0,04, 1); feedback ×0,96 com Hi Cut no laço; chorus com LFO em quadratura L/R | atrasos-base, lei do Golden Ratio |
| Pitch | 2 cabeças rotativas com janelas Hann complementares (sen² + cos² = 1); janela 25 ms (Detune), 45 ms (Shifter/Octaver 1 oct), 60 ms (2 oct); Shifter: realimentação pós-atraso (cada repetição soma o intervalo) | tamanhos de janela |
| Delay | linhas de 2,1 s (Hermite), Hi Cut (1 polo) e Lo Cut (1 polo HP) no feedback, FB máx. 0,99, limitador suave acima de 0 dBFS na entrada da linha | — |
| Dynamic Delay | envelope 1/60 ms; acima de Sensitivity ⇒ alvo −Damping dB, ataque 10 ms, retorno com Release | ataque |
| Reverb | pré-delay → 12 taps de reflexão (5–80 ms × Size; envelope por Shape) → 4 difusores all-pass (k por Diffuse) → FDN 8 linhas (29,7–79,3 ms × Size × algoritmo), ganhos por RT60, LP de amortecimento (Hi Color/Factor), Hadamard ortonormal; saída com low-shelf de 250 Hz (Lo Color/Factor); modulação leve em Hall/Plate; Spring com 6 all-pass dispersivos k=0,68 | todas as escalas por algoritmo |

## Segurança numérica

- `flushDenormal` em todos os estados recursivos + `ScopedNoDenormals` no plugin.
- Feedbacks limitados (< 1), FDN ortonormal com ganhos < 1, all-pass com |k| < 1.
- NaN/Inf na saída ⇒ bloco zerado e DSP resetado (contador exposto para testes).
