# Histórico de versões

## 0.9.1 — correções e sugestões do usuário

### Volumes com faixa de −50 a +50 dB
- **OUT LEVEL** do patch, **OUT LEVEL** de cada bloco (Filter/Mod, Pitch, Chorus/Flange, Delay,
  Reverb), **INPUT** e **GLOBAL OUT** vão agora de **−50 dB a +50 dB** (antes −100…0 dB). Isso dá
  margem para subir o volume de efeitos que perdem nível na cadeia.
- O knob mostra o arco a partir de 0 dB (centro).
- **Compatibilidade com o hardware:** o G•Major só aceita −100…0 dB nesses campos.
  - No envio por MIDI (Tx), valores acima de 0 dB são limitados a 0 dB, e a barra de status avisa.
  - Nos arquivos `.syx`, o preset principal continua válido para o hardware. O valor real segue
    numa mensagem extra `F0 7D 'G' 'M' 'R' …`, que o G•Major ignora e o plugin lê.
  - Um preset vindo do hardware com nível "Off" (−100 dB) é carregado como −50 dB.
- **Modificadores (pedal) em OUT LEVEL** continuam com a faixa do hardware: pedal no mínimo =
  mudo, no máximo = 0 dB. Assim um pedal nunca leva o volume a +50 dB.

### Racks VST3 individuais
Oito plugins VST3 novos, um por bloco, para usar em estágios diferentes da cadeia de sinal:

| Plugin | Bloco |
|---|---|
| G-Major Noise Gate Rack | Noise Gate |
| G-Major EQ Rack | EQ de 3 bandas (acrescentado: no G•Major o EQ é um bloco à parte) |
| G-Major Compressor Rack | Compressor |
| G-Major Filter-Mod Rack | Filter/Mod (6 algoritmos) |
| G-Major Pitch Rack | Pitch (4 algoritmos) |
| G-Major Chorus-Flange Rack | Chorus/Flanger (5 algoritmos) |
| G-Major Delay Rack | Delay (3 algoritmos) |
| G-Major Reverb Rack | Reverb (4 algoritmos) |

Cada rack tem o mesmo painel do bloco no plugin completo, mais:
- INPUT, OUTPUT (−50…+50 dB), BYPASS e medidores de entrada e saída (o Gate e o Compressor
  também mostram a atenuação);
- KILL DRY nos blocos com Mix, para usar em envio auxiliar 100% wet;
- TAP/TEMPO SRC nos blocos com tempo (Filter/Mod, Chorus/Flange, Delay);
- M1–M4 com MIDI learn nos blocos com modificadores;
- lista de presets do bloco e "Open .syx", que pega as configurações do bloco num preset do G•Major.

O instalador tem um componente novo, "Racks VST3 individuais", marcado por padrão.

### Correções internas
- Um teste de estabilidade passou a contar ganhos legítimos de +50 dB como instabilidade. Ele
  agora separa ganho de oscilação, e um teste novo garante que o ganho máximo empilhado
  (8 × +50 dB) continua finito.

## 0.9.0 — primeira versão
Reconstrução completa: core DSP, plugin VST3 + Standalone, codec SysEx do G•Major, testes,
instalador.
