# PerfPulse

Gravador de desempenho de jogos para Windows, leve e sem injeção: fica na bandeja,
percebe sozinho quando um jogo começa, grava FPS/frametime e sensores durante a
partida e, quando o jogo fecha, gera um **relatório HTML offline** com resumo,
médias, medianas, nota de suavidade, **diagnóstico de gargalo** (quem foi o
ofensor e quando) e **energia consumida** por GPU e CPU.

- Um único `PerfPulse.exe` (~0,9 MB, C++20/Win32, sem runtime externo).
- ~0,7% de **um** núcleo durante a gravação; ~3 MB de RAM em espera.
- Em CPUs híbridas (Intel 12ª gen+), roda em modo eficiência (EcoQoS) e deixa os P-cores para o jogo.

## Como funciona

| Etapa | Técnica |
|---|---|
| Detectar o jogo | A cada 2 s: janela em primeiro plano no monitor escolhido, cobrindo a tela (tela cheia ou sem bordas) e apresentando ≥ 10 frames/s por *N* segundos. Listas "sempre"/"nunca monitorar" têm prioridade. |
| FPS e frametime | Consumidor ETW em tempo real (DXGI, D3D9 e DxgKrnl, o mesmo método do PresentMon), com filtro por ID de evento no kernel. Cada frame é registrado, o que permite 1% e 0.1% low corretos. |
| GPU NVIDIA | NVML (uso, temperatura, clocks, consumo, ventoinha, VRAM, limitador) + NVAPI (hotspot, junction da VRAM, tensão). |
| GPU AMD/Intel | WDDM: contadores "GPU Engine"/"GPU Adapter Memory" + `D3DKMTQueryAdapterInfo` (temperatura, clocks, ventoinha, tensão). |
| CPU | Uso por núcleo e **thread mais ocupada do jogo** (NtQuerySystemInformation); clock efetivo (PDH); temperatura e consumo via **PawnIO** → HWiNFO → LibreHardwareMonitor. |
| Relatório | Análise em C++, HTML único com gráficos em canvas embutidos (sem internet), PT-BR/EN, claro/escuro. |

Os dados ficam em memória durante o jogo: nada é gravado em disco até a partida acabar.

## Requisitos

- Windows 10 21H2+ ou Windows 11, x64.
- Executar como administrador (o manifesto já pede). ETW de apresentação e PawnIO exigem elevação.
  A opção **Iniciar com o Windows** cria uma tarefa agendada elevada, então não aparece UAC no logon.
- Para temperatura e consumo da CPU: [PawnIO](https://pawnio.eu) instalado + módulos na pasta `modules\` ao lado do exe
  (`tools\fetch_pawnio_modules.ps1`). Alternativas: HWiNFO com *Shared Memory Support* ou LibreHardwareMonitor abertos.

## Compilar

Pré-requisitos: Visual Studio 2022 Build Tools (workload C++, Windows SDK 10.0.22621+, CMake e Ninja).

```powershell
.\tools\fetch_pawnio_modules.ps1   # uma vez: módulos assinados do PawnIO
.\tools\build.ps1                  # Release + testes -> build\release\PerfPulse.exe
.\tools\build.ps1 -Config debug
```

Ou direto com CMake, num *Developer PowerShell*:

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release
```

> O repositório pode ficar num caminho UNC (`\\wsl.localhost\...`); o `build.ps1` mapeia uma letra de unidade
> durante o build, porque CMake e Ninja não lidam bem com UNC.

## Usar

1. Rode `PerfPulse.exe`. Na primeira vez a janela de configurações abre; depois ele vai direto para a bandeja.
2. Jogue. O ícone fica âmbar enquanto confirma o jogo e ganha um ponto vermelho quando está gravando.
3. Feche o jogo (ou use **Encerrar gravação** no menu): o relatório vai para `Documentos\PerfPulse\Reports`
   e o `index.html` dessa pasta compara todas as partidas por jogo.

Bandeja: **duplo clique** abre as configurações; **botão direito** mostra o menu (ativar/pausar, último relatório,
histórico, pasta, relatório de demonstração, sair).

### Linha de comando

| Argumento | Efeito |
|---|---|
| `--autostart` | Usado pela tarefa de logon (não abre a configuração na primeira execução). |
| `--diag [--out arquivo.txt]` | Diagnóstico: fontes de sensores, 3 amostras e taxa de *presents* por processo por 10 s. |
| `--demo-report [--out arquivo.txt]` | Gera um relatório a partir de uma sessão sintética de 18 min. |
| `--exit` | Pede à instância em execução que salve o relatório da partida atual e feche. |

## O relatório

- **Resumo**: jogo, data, duração (tempo ativo × segundo plano), hardware, resolução, API e V-Sync.
- **KPIs**: FPS médio e mediano, 1% e 0.1% low, frametime mediano/p99, stutters por minuto, Wh e FPS por watt.
- **Nota de suavidade** de 0 a 100 (S/A/B/C/D): consistência local (45%), stutters por minuto (35%) e regularidade entre frames (20%).
- **Gargalo**: cada amostra é classificada como GPU, GPU (throttling), CPU, CPU (térmico), VRAM, RAM, limite de FPS,
  indefinido, carregamento ou segundo plano. O relatório mostra a linha do tempo, o ofensor principal e dicas específicas.
- **Stutters**: cada travada, com o horário e o estado do hardware naquele instante.
- **Sensores**: um gráfico por métrica escolhida, com média, mediana, mínimo, máximo e p95.
- **Energia**: Wh de GPU e CPU (somente o que é medido de verdade), potência média e de pico, energia por frame.

## Estrutura

```
src/
  core/       config, JSON, i18n, métricas, caminhos, log
  capture/    consumidor ETW de presents
  detection/  detector de jogo, monitores, informações de processo
  sensors/    NVIDIA, WDDM genérico, CPU, PawnIO/HWiNFO/LHM, memória
  session/    modelo de dados e gravador da sessão
  analysis/   estatísticas de frame, stutter, nota, gargalo, energia
  report/     payload JSON, gerador HTML, histórico, sessão de demonstração
  app/        bandeja, janela de configurações, tema, autostart, diagnóstico
resources/    ícone, manifesto, templates HTML do relatório e do histórico
tests/        testes unitários (ctest) e um "jogo falso" D3D11 para testes ponta a ponta
tools/        build, ícone e download dos módulos PawnIO
docs/         arquitetura e padrão de código
```

Veja [docs/architecture.md](docs/architecture.md) e [docs/coding-style.md](docs/coding-style.md).

## Limitações conhecidas (v0.1)

- Sem overlay durante o jogo (fase 2).
- Em AMD/Intel, o consumo da GPU em watts não é exposto pelo WDDM: fica "não medido" até a integração com ADLX/IGCL.
- A energia considera só GPU e pacote da CPU; placa-mãe, RAM, SSD, ventoinhas e perdas da fonte ficam de fora.
- FPS = frequência de chamadas *Present* da aplicação (como o "FPS" do PresentMon), não frames exibidos pelo monitor.
