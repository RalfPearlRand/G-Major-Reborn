# Formato do preset SysEx do TC Electronic G•Major

Recuperado do editor GLydian (`com.glydian.model.*`). As tabelas abaixo são **geradas** a
partir da tabela de parâmetros do código (`gmr_render --sysex-map`). Por isso nunca divergem
da implementação.

## Estrutura da mensagem (615 bytes)

Offsets em coordenadas de `SysexMessage.getData()` (sem o `F0` inicial); some 1 para o offset
no arquivo `.syx`.

| Offset | Tamanho | Conteúdo | Confiança |
|---|---|---|---|
| — | 1 | `F0` | confirmado |
| 0–4 | 5 | `00 20 1F` (TC Electronic), `00` (device), `48` (G•Major) | confirmado |
| 5 | 1 | `20` = dados de preset (`45` = pedido de preset) | confirmado |
| 6–7 | 2 | número do preset: `lsb + 128·msb` (0 = edit buffer, 1–100 factory, 101–200 user) | inferido |
| 8–27 | 20 | nome ASCII, completado com espaços | confirmado |
| 28–155 | 32 × 4 | slots de modificadores (ver abaixo) | confirmado |
| 156–159 | 4 | campo de bits: routing (bits 0–1), out level + 100 (bits 2–8), relé 1 (bit 9), relé 2 (bit 10) | relés: inconsistente no GLydian |
| 160–611 | 4 cada | parâmetros | confirmado |
| 612 | 1 | checksum = Σ bytes 28..611 `& 0x7F` | confirmado |
| 613 | 1 | `F7` | confirmado |

**Pedido de preset:** `F0 00 20 1F 00 48 45 lsb msb F7`.

**Mensagem de extensão do G-Major Reborn (v0.9.1, opcional, só em arquivos):** quando algum
OUT LEVEL passa da faixa do hardware (> 0 dB), o preset de 615 bytes leva o valor limitado
(válido para o G•Major) e é seguido por
`F0 7D 47 4D 52 01 n {off_lo off_hi v0 v1 v2 v3}×n F7`. `7D` é o ID MIDI de uso não comercial,
que qualquer equipamento ignora. `off` é o offset do campo (156 = OUT LEVEL do patch) e `v` é
um inteiro empacotado como abaixo.

**Inteiro empacotado (4 bytes):** complemento de 2 de 24 bits, 7 bits por byte, LSB primeiro;
o byte 3 guarda os bits 21–23 (`07` ⇒ negativo). Ex.: −6 = `7A 7F 7F 07`, 1800 = `08 0E 00 00`.

**Campo de bits (4 bytes = 28 bits):** byte *i* contém os bits 7*i*…7*i*+6, LSB primeiro.

**Slot de modificador (28 bits):** fonte (bits 0–2: 0 = OFF, 1–4 = M1–M4), MinOut (3–9),
MidOut (10–16), MaxOut (17–23), em %.

**Campos compartilhados:** blocos multi-algoritmo reutilizam os mesmos offsets para algoritmos
diferentes (ex.: 252 = *Freq Max* do Auto Resonance **ou** *Tempo* do Phaser/Tremolo/Panner).
O algoritmo ativo (offset 228/292/356/420/484) decide a interpretação.

## Parâmetros

| Offset (data / .syx) | Block | Key | Label | Algorithms | Range (stored) | Encoding |
|---|---|---|---|---|---|---|
| 156 / 157 | PATCH | `routing` | ROUTING | all | 0..2 (Serial .. Parallel) | bit field @156 |
| 156 / 157 | PATCH | `out_level` | OUT LEVEL | all | -50..50 (-50 dB .. 50 dB) | bit field @156 |
| 156 / 157 | PATCH | `relay1` | RELAY 1 | all | 0..1 (Off .. On) | bit field @156 |
| 156 / 157 | PATCH | `relay2` | RELAY 2 | all | 0..1 (Off .. On) | bit field @156 |
| 160 / 161 | PATCH | `tap_tempo` | TAP TEMPO | all | 100..3000 (100 ms .. 3000 ms) | packed 24-bit signed int |
| 608 / 609 | NOISE GATE | `gate_on` | NOISE GATE | all | 0..1 (Off .. On) | packed int, 0 = ON / 1 = OFF |
| 548 / 549 | NOISE GATE | `gate_mode` | GATE TYPE | all | 0..1 (Soft .. Hard) | packed 24-bit signed int |
| 552 / 553 | NOISE GATE | `gate_thr` | THRESHOLD | all | -60..0 (-60 dB .. 0 dB) | packed 24-bit signed int |
| 556 / 557 | NOISE GATE | `gate_damp` | MAX DAMP | all | 0..90 (0 dB .. 90 dB) | packed 24-bit signed int |
| 560 / 561 | NOISE GATE | `gate_rel` | RELEASE | all | 3..200 (3 dB/sec .. 200 dB/sec) | packed 24-bit signed int |
| 564 / 565 | EQUALIZER | `eq_on` | EQUALIZER | all | 0..1 (Off .. On) | packed 24-bit signed int |
| 568 / 569 | EQUALIZER | `eq1_freq` | FREQ | all | 25..241 (40.97 Hz .. Off) | packed int = table index + offset |
| 572 / 573 | EQUALIZER | `eq1_gain` | GAIN | all | -12..12 (-12 dB .. 12 dB) | packed 24-bit signed int |
| 576 / 577 | EQUALIZER | `eq1_width` | WIDTH | all | 3..16 (0.2 oct .. 4.0 oct) | packed int = table index + offset |
| 580 / 581 | EQUALIZER | `eq2_freq` | FREQ | all | 25..241 (40.97 Hz .. Off) | packed int = table index + offset |
| 584 / 585 | EQUALIZER | `eq2_gain` | GAIN | all | -12..12 (-12 dB .. 12 dB) | packed 24-bit signed int |
| 588 / 589 | EQUALIZER | `eq2_width` | WIDTH | all | 3..16 (0.2 oct .. 4.0 oct) | packed int = table index + offset |
| 592 / 593 | EQUALIZER | `eq3_freq` | FREQ | all | 25..241 (40.97 Hz .. Off) | packed int = table index + offset |
| 596 / 597 | EQUALIZER | `eq3_gain` | GAIN | all | -12..12 (-12 dB .. 12 dB) | packed 24-bit signed int |
| 600 / 601 | EQUALIZER | `eq3_width` | WIDTH | all | 3..16 (0.2 oct .. 4.0 oct) | packed int = table index + offset |
| 224 / 225 | COMPRESSOR | `comp_on` | COMPRESSOR | all | 0..1 (Off .. On) | packed int, 0 = ON / 1 = OFF |
| 164 / 165 | COMPRESSOR | `comp_thr` | THRESHOLD | all | -40..0 (-40 dB .. 0 dB) | packed 24-bit signed int |
| 168 / 169 | COMPRESSOR | `comp_ratio` | RATIO | all | 0..15 (Off .. Inf:1) | packed int = table index + offset |
| 172 / 173 | COMPRESSOR | `comp_attack` | ATTACK | all | 3..14 (1.0 ms .. 70 ms) | packed int = table index + offset |
| 176 / 177 | COMPRESSOR | `comp_release` | RELEASE | all | 13..23 (50 ms .. 2000 ms) | packed int = table index + offset |
| 180 / 181 | COMPRESSOR | `comp_gain` | GAIN | all | -6..6 (-6 dB .. 6 dB) | packed 24-bit signed int |
| 288 / 289 | FILTER/MOD | `flt_on` | FILTER/MOD | all | 0..1 (Off .. On) | packed int, 0 = ON / 1 = OFF |
| 228 / 229 | FILTER/MOD | `flt_algo` | ALGORITHM | all | 0..5 (AUTO RESONANCE .. PANNER) | packed 24-bit signed int |
| 280 / 281 | FILTER/MOD | `flt_mix` | MIX | AUTO RESONANCE, RESONANCE FILTER, VINTAGE PHASER, SMOOTH PHASER | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 284 / 285 | FILTER/MOD | `flt_out` | OUT LEVEL | all | -50..50 (-50 dB .. 50 dB) | packed 24-bit signed int |
| 232 / 233 | FILTER/MOD | `flt_order` | ORDER | AUTO RESONANCE, RESONANCE FILTER | 0..1 (2nd .. 4th) | packed 24-bit signed int |
| 236 / 237 | FILTER/MOD | `flt_sens` | SENSITIVITY | AUTO RESONANCE | 0..10 (0 .. 10) | packed 24-bit signed int |
| 240 / 241 | FILTER/MOD | `flt_resp` | RESPONSE | AUTO RESONANCE | 0..2 (Slow .. Fast) | packed 24-bit signed int |
| 252 / 253 | FILTER/MOD | `flt_fmax` | FREQ MAX | AUTO RESONANCE | 34..54 (1.00k Hz .. 10.0k Hz) | packed int = table index + offset |
| 272 / 273 | FILTER/MOD | `flt_rhicut` | HI CUT | RESONANCE FILTER | 18..57 (158.5 Hz .. 14.1k Hz) | packed int = table index + offset |
| 276 / 277 | FILTER/MOD | `flt_reso` | HI RESO | RESONANCE FILTER | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 244 / 245 | FILTER/MOD | `flt_speed` | SPEED | VINTAGE PHASER, SMOOTH PHASER, TREMOLO, PANNER | 0..208 (.050 Hz .. 19.95 Hz) | packed int = table index + offset |
| 248 / 249 | FILTER/MOD | `flt_depth` | DEPTH | VINTAGE PHASER, SMOOTH PHASER, TREMOLO, PANNER | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 252 / 253 | FILTER/MOD | `flt_tempo` | TEMPO | VINTAGE PHASER, SMOOTH PHASER, TREMOLO, PANNER | 0..16 (Ignored .. 1/32T) | packed int = table index + offset |
| 260 / 261 | FILTER/MOD | `flt_fb` | FB | VINTAGE PHASER, SMOOTH PHASER | -100..100 (-100 .. 100) | packed 24-bit signed int |
| 264 / 265 | FILTER/MOD | `flt_range` | RANGE | VINTAGE PHASER, SMOOTH PHASER | 0..1 (Low .. High) | packed 24-bit signed int |
| 268 / 269 | FILTER/MOD | `flt_phrev` | PHASE REVERSE | VINTAGE PHASER, SMOOTH PHASER | 0..1 (Off .. On) | packed int, 0 = ON / 1 = OFF |
| 256 / 257 | FILTER/MOD | `flt_ttype` | TYPE | TREMOLO | 0..1 (Soft .. Hard) | packed 24-bit signed int |
| 260 / 261 | FILTER/MOD | `flt_pw` | PULSE WIDTH | TREMOLO | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 272 / 273 | FILTER/MOD | `flt_thicut` | HI CUT | TREMOLO | 0..60 (19.95 Hz .. Off) | packed int = table index + offset |
| 352 / 353 | PITCH | `pit_on` | PITCH | all | 0..1 (Off .. On) | packed int, 0 = ON / 1 = OFF |
| 292 / 293 | PITCH | `pit_algo` | ALGORITHM | all | 0..3 (DETUNE .. PITCH SHIFTER) | packed 24-bit signed int |
| 340 / 341 | PITCH | `pit_mix` | MIX | DETUNE, OCTAVER, PITCH SHIFTER | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 344 / 345 | PITCH | `pit_out` | OUT LEVEL | all | -50..50 (-50 dB .. 50 dB) | packed 24-bit signed int |
| 296 / 297 | PITCH | `det_v1` | VOICE 1 | DETUNE | -100..100 (-100 cents .. 100 cents) | packed 24-bit signed int |
| 300 / 301 | PITCH | `det_v2` | VOICE 2 | DETUNE | -100..100 (-100 cents .. 100 cents) | packed 24-bit signed int |
| 312 / 313 | PITCH | `det_d1` | DELAY 1 | DETUNE | 0..50 (0 ms .. 50 ms) | packed 24-bit signed int |
| 316 / 317 | PITCH | `det_d2` | DELAY 2 | DETUNE | 0..50 (0 ms .. 50 ms) | packed 24-bit signed int |
| 328 / 329 | PITCH | `wh_pitch` | PITCH | WHAMMY | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 332 / 333 | PITCH | `pit_dir` | DIRECTION | WHAMMY, OCTAVER | 0..1 (Down .. Up) | packed 24-bit signed int |
| 336 / 337 | PITCH | `pit_range` | RANGE | WHAMMY, OCTAVER | 1..2 (1 Oct .. 2 Oct) | packed 24-bit signed int |
| 296 / 297 | PITCH | `shf_v1` | VOICE 1 | PITCH SHIFTER | -2400..2400 (-2400 cents .. 2400 cents) | packed 24-bit signed int |
| 300 / 301 | PITCH | `shf_v2` | VOICE 2 | PITCH SHIFTER | -2400..2400 (-2400 cents .. 2400 cents) | packed 24-bit signed int |
| 304 / 305 | PITCH | `shf_pan1` | PAN 1 | PITCH SHIFTER | -50..50 (50L .. 50R) | packed 24-bit signed int |
| 308 / 309 | PITCH | `shf_pan2` | PAN 2 | PITCH SHIFTER | -50..50 (50L .. 50R) | packed 24-bit signed int |
| 312 / 313 | PITCH | `shf_d1` | DELAY 1 | PITCH SHIFTER | 0..350 (0 ms .. 350 ms) | packed 24-bit signed int |
| 316 / 317 | PITCH | `shf_d2` | DELAY 2 | PITCH SHIFTER | 0..350 (0 ms .. 350 ms) | packed 24-bit signed int |
| 320 / 321 | PITCH | `shf_fb1` | FB 1 | PITCH SHIFTER | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 324 / 325 | PITCH | `shf_fb2` | FB 2 | PITCH SHIFTER | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 328 / 329 | PITCH | `shf_l1` | LEVEL 1 | PITCH SHIFTER | -100..0 (-100 dB .. 0 dB) | packed 24-bit signed int |
| 332 / 333 | PITCH | `shf_l2` | LEVEL 2 | PITCH SHIFTER | -100..0 (-100 dB .. 0 dB) | packed 24-bit signed int |
| 416 / 417 | CHORUS/FLANGE | `mod_on` | CHORUS/FLANGE | all | 0..1 (Off .. On) | packed int, 0 = ON / 1 = OFF |
| 356 / 357 | CHORUS/FLANGE | `mod_algo` | ALGORITHM | all | 0..4 (CLASSIC CHORUS .. VIBRATO) | packed 24-bit signed int |
| 396 / 397 | CHORUS/FLANGE | `mod_mix` | MIX | CLASSIC CHORUS, ADVANCED CHORUS, CLASSIC FLANGE, ADVANCED FLANGE | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 400 / 401 | CHORUS/FLANGE | `mod_out` | OUT LEVEL | all | -50..50 (-50 dB .. 50 dB) | packed 24-bit signed int |
| 360 / 361 | CHORUS/FLANGE | `mod_speed` | SPEED | all | 0..208 (.050 Hz .. 19.95 Hz) | packed int = table index + offset |
| 364 / 365 | CHORUS/FLANGE | `mod_depth` | DEPTH | all | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 368 / 369 | CHORUS/FLANGE | `mod_tempo` | TEMPO | all | 0..16 (Ignored .. 1/32T) | packed int = table index + offset |
| 372 / 373 | CHORUS/FLANGE | `mod_hicut` | HI CUT | all | 0..60 (19.95 Hz .. Off) | packed int = table index + offset |
| 376 / 377 | CHORUS/FLANGE | `mod_fb` | FB | CLASSIC FLANGE, ADVANCED FLANGE | -100..100 (-100 .. 100) | packed 24-bit signed int |
| 380 / 381 | CHORUS/FLANGE | `mod_fbhicut` | FB HI CUT | CLASSIC FLANGE, ADVANCED FLANGE | 0..60 (19.95 Hz .. Off) | packed int = table index + offset |
| 384 / 385 | CHORUS/FLANGE | `mod_delay` | DELAY | ADVANCED CHORUS, ADVANCED FLANGE | 1..500 (1 /10 ms .. 500 /10 ms) | packed 24-bit signed int |
| 388 / 389 | CHORUS/FLANGE | `mod_gold` | GOLDEN RATIO | ADVANCED CHORUS, ADVANCED FLANGE | 0..1 (Off .. On) | packed 24-bit signed int |
| 392 / 393 | CHORUS/FLANGE | `mod_phrev` | PHASE REVERSE | ADVANCED CHORUS, ADVANCED FLANGE | 0..1 (Off .. On) | packed 24-bit signed int |
| 480 / 481 | DELAY | `dly_on` | DELAY | all | 0..1 (Off .. On) | packed int, 0 = ON / 1 = OFF |
| 420 / 421 | DELAY | `dly_algo` | ALGORITHM | all | 0..2 (PING PONG .. DUAL) | packed 24-bit signed int |
| 472 / 473 | DELAY | `dly_mix` | MIX | all | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 476 / 477 | DELAY | `dly_out` | OUT LEVEL | all | -50..50 (-50 dB .. 50 dB) | packed 24-bit signed int |
| 424 / 425 | DELAY | `dly_time` | TIME | all | 0..1800 (0 ms .. 1800 ms) | packed 24-bit signed int |
| 432 / 433 | DELAY | `dly_tempo` | TEMPO | all | 0..16 (Ignored .. 1/32T) | packed int = table index + offset |
| 440 / 441 | DELAY | `dly_fb` | FB | all | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 448 / 449 | DELAY | `dly_fbhicut` | FB HI CUT | all | 40..60 (2.00k Hz .. Off) | packed int = table index + offset |
| 452 / 453 | DELAY | `dly_fblocut` | FB LO CUT | all | 0..40 (Off .. 2.00k Hz) | packed int = table index + offset |
| 436 / 437 | DELAY | `dly_width` | WIDTH | PING PONG | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 428 / 429 | DELAY | `dly_time2` | TIME 2 | DUAL | 0..1800 (0 ms .. 1800 ms) | packed 24-bit signed int |
| 436 / 437 | DELAY | `dly_tempo2` | TEMPO 2 | DUAL | 0..16 (Ignored .. 1/32T) | packed int = table index + offset |
| 444 / 445 | DELAY | `dly_fb2` | FB 2 | DUAL | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 456 / 457 | DELAY | `dly_pan1` | PAN 1 | DUAL | -50..50 (50L .. 50R) | packed 24-bit signed int |
| 460 / 461 | DELAY | `dly_pan2` | PAN 2 | DUAL | -50..50 (50L .. 50R) | packed 24-bit signed int |
| 456 / 457 | DELAY | `dly_offset` | OFFSET | DYNAMIC | -200..200 (-200 ms .. 200 ms) | packed 24-bit signed int |
| 460 / 461 | DELAY | `dly_sense` | SENSIT | DYNAMIC | -50..0 (-50 dB .. 0 dB) | packed 24-bit signed int |
| 464 / 465 | DELAY | `dly_damp` | DAMP | DYNAMIC | 0..100 (0 dB .. 100 dB) | packed 24-bit signed int |
| 468 / 469 | DELAY | `dly_rel` | RELEASE | DYNAMIC | 11..21 (20 ms .. 1000 ms) | packed int = table index + offset |
| 544 / 545 | REVERB | `rev_on` | REVERB | all | 0..1 (Off .. On) | packed int, 0 = ON / 1 = OFF |
| 484 / 485 | REVERB | `rev_algo` | ALGORITHM | all | 0..3 (SPRING .. PLATE) | packed 24-bit signed int |
| 532 / 533 | REVERB | `rev_mix` | MIX | all | 0..100 (0 % .. 100 %) | packed 24-bit signed int |
| 536 / 537 | REVERB | `rev_out` | OUT LEVEL | all | -50..50 (-50 dB .. 50 dB) | packed 24-bit signed int |
| 488 / 489 | REVERB | `rev_decay` | DECAY | all | 1..200 (1 /10 s .. 200 /10 s) | packed 24-bit signed int |
| 492 / 493 | REVERB | `rev_predly` | PRE DLY | all | 0..100 (0 ms .. 100 ms) | packed 24-bit signed int |
| 496 / 497 | REVERB | `rev_shape` | SHAPE | all | 0..2 (Round .. Square) | packed 24-bit signed int |
| 500 / 501 | REVERB | `rev_size` | SIZE | all | 0..7 (Box .. Huge) | packed int = table index + offset |
| 504 / 505 | REVERB | `rev_hicol` | HI COL | all | 0..6 (Wool .. Grand) | packed int = table index + offset |
| 508 / 509 | REVERB | `rev_hifac` | HI FAC | all | -25..25 (-25 .. 25) | packed 24-bit signed int |
| 512 / 513 | REVERB | `rev_locol` | LO COL | all | 0..6 (Thick .. No Base) | packed int = table index + offset |
| 516 / 517 | REVERB | `rev_lofac` | LO FAC | all | -25..25 (-25 .. 25) | packed 24-bit signed int |
| 520 / 521 | REVERB | `rev_roomlvl` | ROOM LVL | all | -100..0 (-100 dB .. 0 dB) | packed 24-bit signed int |
| 524 / 525 | REVERB | `rev_lvl` | REVB LVL | all | -100..0 (-100 dB .. 0 dB) | packed 24-bit signed int |
| 528 / 529 | REVERB | `rev_diffuse` | DIFUSE | all | -25..25 (-25 .. 25) | packed 24-bit signed int |

### Modifier slots

| Offset | Block | Algorithm | Parameter |
|---|---|---|---|
| 28 | FILTER/MOD | AUTO RESONANCE | `flt_mix` |
| 32 | FILTER/MOD | AUTO RESONANCE | `flt_out` |
| 28 | FILTER/MOD | RESONANCE FILTER | `flt_rhicut` |
| 32 | FILTER/MOD | RESONANCE FILTER | `flt_reso` |
| 36 | FILTER/MOD | RESONANCE FILTER | `flt_mix` |
| 40 | FILTER/MOD | RESONANCE FILTER | `flt_out` |
| 28 | FILTER/MOD | VINTAGE PHASER | `flt_speed` |
| 32 | FILTER/MOD | VINTAGE PHASER | `flt_depth` |
| 36 | FILTER/MOD | VINTAGE PHASER | `flt_fb` |
| 40 | FILTER/MOD | VINTAGE PHASER | `flt_mix` |
| 44 | FILTER/MOD | VINTAGE PHASER | `flt_out` |
| 28 | FILTER/MOD | SMOOTH PHASER | `flt_speed` |
| 32 | FILTER/MOD | SMOOTH PHASER | `flt_depth` |
| 36 | FILTER/MOD | SMOOTH PHASER | `flt_fb` |
| 40 | FILTER/MOD | SMOOTH PHASER | `flt_mix` |
| 44 | FILTER/MOD | SMOOTH PHASER | `flt_out` |
| 28 | FILTER/MOD | TREMOLO | `flt_speed` |
| 32 | FILTER/MOD | TREMOLO | `flt_depth` |
| 36 | FILTER/MOD | TREMOLO | `flt_thicut` |
| 40 | FILTER/MOD | TREMOLO | `flt_out` |
| 28 | FILTER/MOD | PANNER | `flt_speed` |
| 32 | FILTER/MOD | PANNER | `flt_depth` |
| 36 | FILTER/MOD | PANNER | `flt_out` |
| 48 | PITCH | DETUNE | `pit_mix` |
| 52 | PITCH | DETUNE | `pit_out` |
| 48 | PITCH | WHAMMY | `wh_pitch` |
| 52 | PITCH | WHAMMY | `pit_out` |
| 48 | PITCH | OCTAVER | `pit_mix` |
| 52 | PITCH | OCTAVER | `pit_out` |
| 48 | PITCH | PITCH SHIFTER | `shf_v1` |
| 52 | PITCH | PITCH SHIFTER | `shf_v2` |
| 56 | PITCH | PITCH SHIFTER | `shf_pan1` |
| 60 | PITCH | PITCH SHIFTER | `shf_pan2` |
| 64 | PITCH | PITCH SHIFTER | `shf_fb1` |
| 68 | PITCH | PITCH SHIFTER | `shf_fb2` |
| 72 | PITCH | PITCH SHIFTER | `pit_mix` |
| 76 | PITCH | PITCH SHIFTER | `pit_out` |
| 80 | CHORUS/FLANGE | CLASSIC CHORUS | `mod_speed` |
| 84 | CHORUS/FLANGE | CLASSIC CHORUS | `mod_depth` |
| 88 | CHORUS/FLANGE | CLASSIC CHORUS | `mod_hicut` |
| 92 | CHORUS/FLANGE | CLASSIC CHORUS | `mod_mix` |
| 96 | CHORUS/FLANGE | CLASSIC CHORUS | `mod_out` |
| 80 | CHORUS/FLANGE | ADVANCED CHORUS | `mod_speed` |
| 84 | CHORUS/FLANGE | ADVANCED CHORUS | `mod_depth` |
| 88 | CHORUS/FLANGE | ADVANCED CHORUS | `mod_hicut` |
| 92 | CHORUS/FLANGE | ADVANCED CHORUS | `mod_mix` |
| 96 | CHORUS/FLANGE | ADVANCED CHORUS | `mod_out` |
| 80 | CHORUS/FLANGE | CLASSIC FLANGE | `mod_speed` |
| 84 | CHORUS/FLANGE | CLASSIC FLANGE | `mod_depth` |
| 88 | CHORUS/FLANGE | CLASSIC FLANGE | `mod_hicut` |
| 92 | CHORUS/FLANGE | CLASSIC FLANGE | `mod_fb` |
| 96 | CHORUS/FLANGE | CLASSIC FLANGE | `mod_fbhicut` |
| 100 | CHORUS/FLANGE | CLASSIC FLANGE | `mod_mix` |
| 104 | CHORUS/FLANGE | CLASSIC FLANGE | `mod_out` |
| 80 | CHORUS/FLANGE | ADVANCED FLANGE | `mod_speed` |
| 84 | CHORUS/FLANGE | ADVANCED FLANGE | `mod_depth` |
| 88 | CHORUS/FLANGE | ADVANCED FLANGE | `mod_hicut` |
| 92 | CHORUS/FLANGE | ADVANCED FLANGE | `mod_fb` |
| 96 | CHORUS/FLANGE | ADVANCED FLANGE | `mod_fbhicut` |
| 100 | CHORUS/FLANGE | ADVANCED FLANGE | `mod_mix` |
| 104 | CHORUS/FLANGE | ADVANCED FLANGE | `mod_out` |
| 80 | CHORUS/FLANGE | VIBRATO | `mod_speed` |
| 84 | CHORUS/FLANGE | VIBRATO | `mod_depth` |
| 88 | CHORUS/FLANGE | VIBRATO | `mod_hicut` |
| 92 | CHORUS/FLANGE | VIBRATO | `mod_out` |
| 108 | DELAY | PING PONG | `dly_time` |
| 112 | DELAY | PING PONG | `dly_fb` |
| 116 | DELAY | PING PONG | `dly_fbhicut` |
| 120 | DELAY | PING PONG | `dly_fblocut` |
| 124 | DELAY | PING PONG | `dly_mix` |
| 128 | DELAY | PING PONG | `dly_out` |
| 108 | DELAY | DYNAMIC | `dly_time` |
| 112 | DELAY | DYNAMIC | `dly_fb` |
| 116 | DELAY | DYNAMIC | `dly_fbhicut` |
| 120 | DELAY | DYNAMIC | `dly_fblocut` |
| 124 | DELAY | DYNAMIC | `dly_mix` |
| 128 | DELAY | DYNAMIC | `dly_out` |
| 108 | DELAY | DUAL | `dly_time` |
| 112 | DELAY | DUAL | `dly_time2` |
| 116 | DELAY | DUAL | `dly_fb` |
| 120 | DELAY | DUAL | `dly_fb2` |
| 124 | DELAY | DUAL | `dly_fbhicut` |
| 128 | DELAY | DUAL | `dly_fblocut` |
| 132 | DELAY | DUAL | `dly_pan1` |
| 136 | DELAY | DUAL | `dly_pan2` |
| 140 | DELAY | DUAL | `dly_mix` |
| 144 | DELAY | DUAL | `dly_out` |
| 148 | REVERB | SPRING | `rev_mix` |
| 152 | REVERB | SPRING | `rev_out` |
| 148 | REVERB | HALL | `rev_mix` |
| 152 | REVERB | HALL | `rev_out` |
| 148 | REVERB | ROOM | `rev_mix` |
| 152 | REVERB | ROOM | `rev_out` |
| 148 | REVERB | PLATE | `rev_mix` |
| 152 | REVERB | PLATE | `rev_out` |
