# Como contribuir

Obrigado pelo interesse no G-Major Reborn. Relatos de bugs, sugestões e melhorias são bem-vindos.

## Antes de enviar uma alteração

1. Abra uma issue para discutir mudanças grandes ou novos algoritmos.
2. Para correções pequenas, envie um pull request descrevendo o problema e a solução.
3. Evite incluir arquivos gerados, builds, binários ou bibliotecas de terceiros sem necessidade.
4. Mantenha documentadas as diferenças entre comportamento confirmado, inferido e ainda não verificado no hardware.

## Compilar e testar o core

É necessário CMake 3.22 ou superior e um compilador C++17.

```bash
cmake -S . -B build -DGMR_BUILD_PLUGIN=OFF
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

O plugin JUCE pode ser compilado separadamente conforme o guia de [instalação e compilação](INSTALL.md). A integração contínua executa os testes do core em Linux e Windows.
