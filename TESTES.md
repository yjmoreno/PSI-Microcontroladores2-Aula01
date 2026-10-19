# Plano de Testes — Atividade Baremetal

**Placa:** FRDM-KL25Z · **Zephyr:** 4.2 · **Branch:** `baremetal`

**Objetivo:** verificar que o LED verde (`led0`) pisca a cada 1000 ms e o LED azul (`led1`)
a cada 350 ms, usando um super-loop (sem threads).

> "Piscar a cada X ms" significa que o LED **troca de estado** a cada X ms
> (aceso X ms, apagado X ms).

## Pré-requisitos
- VS Code + PlatformIO instalados
- Placa FRDM-KL25Z conectada pela porta USB **OpenSDA**
- Cronômetro (celular)
- (Opcional) Analisador lógico ou osciloscópio

## Casos de teste

| # | Teste | Como executar | Resultado esperado |
|---|-------|---------------|--------------------|
| T1 | Compilação | `pio run` | Termina com `[SUCCESS]`, sem erros nem warnings no `main.c` |
| T2 | Gravação | `pio run -t upload` | Upload concluído sem erros |
| T3 | Mensagem inicial | `pio device monitor` e apertar RESET | Aparece `LED0 a cada 1000 ms, LED1 a cada 350 ms` |
| T4 | Estado inicial | Apertar RESET e observar logo depois | Os dois LEDs começam **apagados** |
| T5 | Período do LED verde | Cronometrar 10 trocas de estado do LED verde | Cerca de **10 s** (±0,5 s) |
| T6 | Período do LED azul | Cronometrar 20 trocas de estado do LED azul | Cerca de **7 s** (20 × 350 ms, ±0,5 s) |
| T7 | Independência | Observar os dois LEDs por 30 s | Cada LED mantém seu ritmo; o azul pisca cerca de 3× mais rápido que o verde |
| T8 | Estabilidade | Deixar rodando por 5 min e repetir T5 e T6 | Mesmos resultados; sem travar nem atrasar |
| T9 | Parte básica (500 ms) | Mudar `PERIODO_LED1_MS` para `500`, gravar e observar | O azul troca 2× para cada troca do verde, sempre sincronizados |
| T10 | (Opcional) Medição precisa | Analisador lógico nos pinos PTB19 (verde) e PTD1 (azul) | Meio período de 1000 ms e 350 ms, com erro de ±2 ms |

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
