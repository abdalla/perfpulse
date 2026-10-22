# Arquitetura

## Visão geral

```
            ┌──────────── UI thread ─────────────┐
 Explorer ─▶│ TrayIcon ◀─ Application ─▶ Settings │
            │              │   ▲ tick 2 s          │
            └──────────────┼───┼──────────────────┘
                           │   │ takeActivity()
         ┌─────────────────▼───┴───────────┐
         │ FrameCapture (ETW consumer thr.)│  presents de todos os processos
         └─────────────────┬───────────────┘
              confirmed    │ beginRecording(pid)
         ┌─────────────────▼───────────────┐
         │ SessionRecorder                 │
         │   sampler thread ─▶ SensorHub   │  GPU / CPU / PawnIO / RAM
         └─────────────────┬───────────────┘
              game exited  │ SessionData (em memória)
         ┌─────────────────▼───────────────┐
         │ report thread                   │
         │  analyzeSession ─▶ payload JSON │
         │  ─▶ template HTML ─▶ arquivo    │
         │  ─▶ history.jsonl ─▶ index.html │
         └─────────────────────────────────┘
```

## Threads

| Thread | Vida | Trabalho |
|---|---|---|
| UI | sempre | mensagens, bandeja, janela de configurações, timer de detecção (2 s) |
| ETW consumer | monitoramento ativo | `ProcessTrace`; conta *presents* por processo e guarda os timestamps do jogo gravado |
| Sampler | só durante a gravação | lê os sensores no intervalo configurado, com agenda absoluta e sem deriva |
| Report | após cada partida | análise e escrita do HTML; avisa a UI com `PostMessage` |
| Probe | uma vez no início | inicializa os backends de sensores para mostrar as fontes nas configurações |

## Decisões

- **ETW em vez de hook/injeção**: não toca no processo do jogo (anti-cheat), funciona com qualquer API e custa quase nada.
  O filtro `EVENT_FILTER_TYPE_EVENT_ID` faz o kernel descartar todos os eventos que não são *present*.
- **Fonte preferida por frame**: DXGI > D3D9 > DxgKrnl. Uma fonte de nível de API só é escolhida se viu pelo menos
  metade dos frames da fonte mais completa. Assim jogos Vulkan/OpenGL caem automaticamente no DxgKrnl.
- **Sensores sob demanda**: só o que foi selecionado (mais o necessário para o diagnóstico) é lido. O `SensorHub`
  é criado na thread do sampler e destruído no fim da partida, liberando NVML, PDH e WMI.
- **Backends em cascata**: NVIDIA (NVML+NVAPI) → WDDM genérico; PawnIO → HWiNFO → LibreHardwareMonitor.
  Todas as DLLs são carregadas em tempo de execução: nenhuma dependência de fabricante é linkada.
- **Relatório = template + dados**: o HTML/JS do relatório é um recurso RCDATA; o C++ só injeta o JSON. O template
  é editável sem tocar na análise, e o arquivo final é autossuficiente.
- **`perfpulse_core` testável**: config, JSON, análise e geração de payload não dependem da UI e têm testes unitários.

## Classificação de gargalo (por amostra)

Em ordem de prioridade:

1. **Segundo plano**: o jogo não está em primeiro plano.
2. **Carregamento**: menos de 8 FPS na amostra.
3. **VRAM**: ≥ 97% em uso. **RAM**: ≥ 95% em uso.
4. **Limite de FPS**: FPS dentro de ±2% de um platô que ocupa ≥ 20% do tempo ativo (ou da taxa do monitor), sem GPU nem thread saturadas.
5. **GPU**: uso ≥ 95%. Vira **GPU (throttling)** se o driver reporta limitação térmica ou de hardware.
6. **CPU**: GPU < 85% e (thread mais ocupada do jogo ≥ 85% de um núcleo, ou núcleo mais ocupado ≥ 95%, ou CPU total ≥ 90%).
   Vira **CPU (térmico)** se o pacote estiver a 3 °C do TjMax.
7. **Indefinido**: nada saturado (limites do motor, streaming, sincronização).

O ofensor é a categoria de recurso com maior parcela do tempo ativo. O limite de FPS só vira o veredito quando domina a partida.
