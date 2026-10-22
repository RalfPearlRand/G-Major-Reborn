# Licença

G-Major Reborn — Copyright (C) 2026 os autores do projeto.

Este programa é software livre: você pode redistribuí-lo e/ou modificá-lo sob os termos da
**GNU General Public License, versão 3** ou (a seu critério) qualquer versão posterior,
publicada pela Free Software Foundation. Texto completo: <https://www.gnu.org/licenses/gpl-3.0.txt>.

SPDX-License-Identifier: GPL-3.0-or-later

## Componentes de terceiros

| Componente | Licença | Observação |
|---|---|---|
| JUCE 7.0.12 | GPLv3 (ou comercial) | baixado no build; o splash screen é desativado sob a GPL |
| Steinberg VST3 SDK (incluído no JUCE) | GPLv3 (ou proprietária Steinberg) | |
| Steinberg ASIO SDK | licença Steinberg | **não incluído**; opcional, baixado pelo usuário |
| llvm-mingw | Apache-2.0 com exceções LLVM / ISC | só para compilar; o runtime está linkado estaticamente |

## Material de referência

O editor GLydian e o manual do G•Major foram usados apenas como **referência** para
interoperabilidade (formato de preset e comportamento documentado). Nenhum código do GLydian
e nenhum dado de preset da TC Electronic foi copiado para este projeto. Os 20 presets incluídos
foram criados para este projeto.
