# Plano de Testes — Atividade Threads

**Placa:** FRDM-KL25Z · **Zephyr:** 4.2 · **Branch:** `threads`

**Objetivo:** verificar que o LED verde (`led0`) pisca a cada 1000 ms e o LED azul (`led1`)
a cada 350 ms, cada um controlado por uma **thread** própria (`K_THREAD_DEFINE`).

> "Piscar a cada X ms" significa que o LED **troca de estado** a cada X ms
> (aceso X ms, apagado X ms).
>
> A placa tem um único LED RGB (D3). O verde e o azul são canais do mesmo LED:
> quando os dois estão acesos, a cor vista é **ciano**.

## Pré-requisitos
- VS Code + PlatformIO instalados
- Placa FRDM-KL25Z conectada pela porta USB **OpenSDA**
- Cronômetro (celular)
- (Opcional) Analisador lógico ou osciloscópio

## Casos de teste

| # | Teste | Como executar | Resultado esperado |
|---|-------|---------------|--------------------|
| T1 | Compilação | `pio run` | Termina com `[SUCCESS]`, sem erros |
| T2 | Gravação | `pio run -t upload` | Upload concluído sem erros |
| T3 | Mensagens iniciais | `pio device monitor` e apertar RESET | Aparecem `LED no pino 19 piscando a cada 1000 ms` e `LED no pino 1 piscando a cada 350 ms` (a ordem pode variar) |
| T4 | Estado inicial | Apertar RESET e observar logo depois | Os dois LEDs acendem logo no início (cor **ciano**), pois cada thread troca o estado antes de dormir |
| T5 | Período do LED verde (monitor) | Comparar os tempos `[xxx ms] LED pino 19 trocou` consecutivos | Diferença de **1000 ms** (±2 ms) |
| T6 | Período do LED azul (monitor) | Comparar os tempos `[xxx ms] LED pino 1 trocou` consecutivos | Diferença de **350 ms** (±2 ms) |
| T7 | Período com cronômetro | Cronometrar 10 trocas do verde e 20 trocas do azul | Cerca de **10 s** e **7 s** (±0,5 s) |
| T8 | Independência das threads | Observar os dois LEDs por 30 s | Cada LED mantém seu ritmo; uma thread dormindo (`k_msleep`) não atrasa a outra |
| T9 | Estabilidade | Deixar rodando por 5 min e repetir T5 e T6 | Mesmos resultados; sem travar |
| T10 | Parte básica (500 ms) | Mudar `PERIODO_LED1_MS` para `500`, gravar e observar | O azul troca 2× para cada troca do verde |

## Registro da execução

| # | Executado por | Data | Resultado obtido | OK? |
|---|---------------|------|------------------|-----|
| T1 | | | | |
| T2 | | | | |
| T3 | | | | |
| T4 | | | | |
| T5 | | | | |
| T6 | | | | |
| T7 | | | | |
| T8 | | | | |
| T9 | | | | |
| T10 | | | | |
