# Padrão de código

Baseado no Google C++ Style Guide, com as adaptações abaixo. O `.clang-format` na raiz aplica a formatação.

## Linguagem e build

- C++20, MSVC com `/W4 /permissive- /utf-8`. O build deve compilar **sem avisos**.
- CMake ≥ 3.25 com presets (`debug`, `release`) e gerador Ninja.
- CRT estático (`/MT`): o executável não depende do redistribuível do VC++.

## Organização

- Um módulo por pasta em `src/` (`core`, `capture`, `detection`, `sensors`, `session`, `analysis`, `report`, `app`),
  com namespace correspondente: `perfpulse::<módulo>`.
- Os includes partem de `src/`: `#include "analysis/frame_statistics.hpp"`.
- Headers usam `#pragma once`. O `app/resource_ids.h` é a exceção, porque o compilador de recursos também o lê.
- Lógica sem dependência de UI vai para o `perfpulse_core` e ganha teste em `tests/`.

## Nomes

| Elemento | Padrão | Exemplo |
|---|---|---|
| Arquivos | `snake_case.hpp/.cpp` | `bottleneck_analyzer.cpp` |
| Tipos, enums | `PascalCase` | `SessionRecorder`, `enum class Bottleneck` |
| Funções e métodos | `camelCase` | `analyzeSession()` |
| Variáveis e parâmetros | `camelCase` | `frameTimesSec` |
| Membros privados | `camelCase_` | `sessionHandle_` |
| Constantes | `kPascalCase` | `kMaxFrameGapMs` |
| Macros | `UPPER_CASE` (evitar) | `TEST_CASE` |

Unidades vão no nome quando não são óbvias: `timeSec`, `intervalMs`, `powerLimitWatts`.

## Práticas

- Texto interno em UTF-8 (`std::string`); `std::wstring` só na fronteira com a API Win32 (`core::toWide/toUtf8`).
- Todo handle tem dono RAII (`core::UniqueHandle`, `core::UniqueModule`, `PdhQuery`...).
- Leituras de sensor ausentes são `NaN` (`session::kNoValue`), nunca zero: o relatório mostra "N/D".
- Nada de alocação, I/O ou log por frame no caminho do ETW; o log é só para mudanças de estado.
- APIs de fabricante e funções não documentadas são carregadas com `GetProcAddress`, falham sem derrubar o app,
  e o motivo vai para o log.
- Comentários explicam o *porquê* (decisões, armadilhas do Windows), não o *o quê*.

## Testes

- `ctest --preset release` roda `perfpulse_tests` (framework mínimo em `tests/test_framework.hpp`).
- `tests/fake_game` gera um executável D3D11 para testar detecção e gravação ponta a ponta:
  `perfpulse_fake_game.exe 45 144` (segundos, FPS alvo; `--windowed` para janela).
