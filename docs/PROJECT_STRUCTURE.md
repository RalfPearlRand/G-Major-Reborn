# Estrutura do projeto

O repositório separa o processamento de áudio, a interface, as ferramentas e a documentação para facilitar manutenção e contribuições.

| Caminho | Conteúdo |
|---|---|
| `core/` | Biblioteca C++17 independente: parâmetros, presets, codec SysEx e DSP |
| `plugin/` | Plugin VST3 e aplicativo Standalone feitos com JUCE |
| `installer/` | Código e recursos do instalador para Windows |
| `presets/` | Presets de demonstração em formato SysEx |
| `tests/` | Testes do core e harness de integração do plugin |
| `tools/` | Renderizador offline, comparação A/B e gerador de tabelas |
| `scripts/` | Scripts de build para Windows e Linux |
| `cmake/` | Toolchain de cross-compilação e stubs usados pelo build |
| `resources/` | Ícones e recursos gráficos |
| `docs/` | Formato SysEx, notas de DSP, capturas e relatórios técnicos |
| `.github/workflows/` | Compilação e testes automatizados |

## Documentos principais

- [Instalação e compilação](../INSTALL.md)
- [Relatório de reconstrução](../RECONSTRUCTION_REPORT.md)
- [Diferenças conhecidas](../KNOWN_DIFFERENCES.md)
- [Notas de DSP](DSP_NOTES.md)
- [Formato SysEx](SYSEX_FORMAT.md)
