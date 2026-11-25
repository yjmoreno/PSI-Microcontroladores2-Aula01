# Atividade Acelerômetro – Serial (threads + integração numérica)

O projeto lê o acelerômetro MMA8451Q da FRDM-KL25Z (I²C0, endereço 0x1D), calcula velocidade e posição por integração numérica e manda tudo pela serial em CSV (`ax,ay,az,vx,vy,vz,px,py,pz`), a 115200 bps e uma linha a cada 50 ms. O sensor é acessado pela API de sensores do Zephyr, com o driver `fxos8700` em modo só acelerômetro (`CONFIG_FXOS8700_MODE_ACCEL=y`), que é o driver que o Zephyr usa para o MMA8451Q dessa placa.

## Como rodar

1. Abrir a pasta no VSCode com PlatformIO e fazer **Upload**.
2. No computador, rodar `python leitor_serial.py` (no Mac, `python leitor_serial.py /dev/cu.usbmodemXXXX`).
3. Com o leitor já conectado, apertar **RESET** e deixar a placa **parada sobre a mesa** até o LED vermelho apagar (1 s de calibração). O leitor percebe o reset e recomeça o arquivo do zero.
4. Mexer a placa, esperar uns 2 s parada e encerrar com Ctrl+C.
5. Rodar `python gerar_graficos.py` para gerar `graficos_atividade2.png`.

## Organização das threads

O trabalho foi dividido em três threads preemptivas, uma para cada etapa, ligadas por duas filas de mensagens (`k_msgq`):

```
aquisição (prio 5) --fila_acel--> integração (prio 6) --fila_serial--> serial (prio 7)
```

| Thread | Prioridade | O que acorda a thread | Onde ela bloqueia | Tempo de CPU por período (estimado) |
| --- | --- | --- | --- | --- |
| aquisição | 5 (maior) | timer periódico de 50 ms | `k_timer_status_sync()` | ≈ 1 ms (leitura I²C a 100 kHz) |
| integração | 6 | chegada de uma amostra na `fila_acel` | `k_msgq_get(&fila_acel)` | dezenas de µs (contas em float) |
| serial | 7 (menor) | chegada de um resultado na `fila_serial` | `k_msgq_get(&fila_serial)` | ≈ 7 ms (~80 caracteres a 115200 bps) |

## Comentários sobre o escalonamento

O critério para as prioridades foi o quanto cada tarefa sofre com atraso. A integração usa um passo fixo de 50 ms, então se a amostragem atrasar ou tremer (jitter), o `dt` real deixa de ser 50 ms e o erro entra direto na velocidade e, integrado de novo, na posição. Por isso a aquisição tem a maior prioridade: quando o timer vence, ela preempta qualquer outra thread e lê o sensor na hora certa. Para o período ficar fixo foi usado um `k_timer` periódico em vez de `k_msleep(50)`: com o `k_msleep` o período real seria 50 ms mais o tempo da leitura, e esse erro iria se acumulando.

A integração vem logo abaixo. Ela não tem período próprio: fica bloqueada na fila e só roda quando chega uma amostra, faz meia dúzia de contas e volta a dormir. A serial ficou com a menor prioridade porque é a única tarefa "pesada": o `printk` da KL25Z é feito por polling, ou seja, a thread fica ocupando a CPU enquanto os caracteres saem, uns 7 ms por linha. Se ela tivesse prioridade maior que a aquisição, uma linha sendo impressa poderia atrasar a leitura do sensor. Como está com prioridade menor, quando o timer vence no meio de uma linha, o escalonador interrompe a serial, a aquisição lê o sensor e a serial continua de onde parou.

Em cada período de 50 ms a sequência fica assim: o timer acorda a aquisição; ela lê o sensor e coloca a amostra na fila, o que deixa a integração pronta, mas a integração só roda quando a aquisição volta a bloquear no timer (prioridade menor); a integração calcula e coloca o resultado na outra fila, liberando a serial; a serial imprime a linha e volta a esperar. No resto do período (bem mais de 40 ms) só a thread idle roda, então a CPU está com folga, algo em torno de 15–20% de ocupação.

Como as três threads têm prioridades diferentes, não foi preciso time slicing (round robin): nunca há duas threads prontas com a mesma prioridade disputando a CPU. Também não há variável compartilhada protegida por mutex, porque os dados passam de uma thread para outra pelas filas (padrão produtor–consumidor). Com isso não aparece o problema de inversão de prioridade visto em aula: nenhuma thread de prioridade alta fica esperando um recurso preso por uma de prioridade baixa. Os `k_msgq_put` usam `K_NO_WAIT`, então se a fila estiver cheia a thread de cima não fica bloqueada esperando a de baixo; a amostra é descartada e contada em `perdidas_acel`/`perdidas_serial` (dá para ver esses contadores no debug). Com 8 posições por fila e a carga acima, na prática a fila não chega a encher.

## Integração numérica

Foi usada a regra dos trapézios, aplicada amostra a amostra:

```
v[k] = v[k-1] + (a[k-1] + a[k]) / 2 · dt
p[k] = p[k-1] + (v[k-1] + v[k]) / 2 · dt        com dt = 0,05 s
```

Ela precisa só da amostra anterior, então dá para calcular em tempo real sem atraso. Simpson seria um pouco mais precisa, mas precisa de pares de intervalos (atualizaria a cada duas amostras), e a quadratura gaussiana pede valores da função em pontos fora da grade de amostragem, o que não temos com o sensor amostrado a passo fixo.

Antes de integrar, a thread de integração faz uma calibração: a média das primeiras 20 amostras (1 s parada) vira o offset de cada eixo e é subtraída das leituras seguintes. Isso tira a gravidade do eixo z e o offset de fábrica do sensor. Além disso, acelerações menores que 0,10 m/s² são consideradas zero (zona morta), para o ruído da placa parada não ser integrado.

Mesmo assim, a posição obtida só pela integração dupla do acelerômetro diverge com o tempo: qualquer erro pequeno de offset vira uma velocidade que cresce linearmente e uma posição que cresce com t². Inclinar a placa também faz parte da gravidade aparecer em x ou y, o que a calibração inicial não corrige. Por isso os resultados fazem sentido em movimentos curtos (alguns segundos), com a placa deslizando sobre a mesa sem girar.

## Resultados na placa

Fiz duas capturas, as duas com a calibração feita com a placa deitada e parada na mesa. Os gráficos e os dados estão na pasta `resultados/`.

**Movimento lento** (`grafico_movimento_lento.png`). Deslizei a placa devagar, com vários ajustes, por uns 25 s. As acelerações ficaram pequenas, quase todas abaixo de 0,5 m/s² e muitas perto da zona morta de 0,1 m/s². Com isso, pedaços da arrancada e da frenagem foram cortados, a velocidade nunca voltou a zero (terminou em −0,28 m/s em x) e a posição foi derivando até quase −6 m, um valor sem sentido físico. Nesse caso o erro pesa mais do que o próprio movimento.

**Movimento rápido** (`grafico_movimento_rapido.png`). Dei um empurrão único e firme. O pulso aparece bem claro perto dos 10 s: `ax` sobe até 1,3 m/s² na arrancada e cai a −3,3 m/s² na frenagem, `vx` faz uma corcova com pico de 0,8 m/s e `px` sobe até uns 0,6 m. Como as acelerações ficaram bem acima da zona morta, o formato do movimento foi capturado. Mesmo assim a velocidade não volta exatamente a zero (sobra −0,17 m/s em x e −0,37 m/s em y, provavelmente porque a placa girou um pouco no empurrão e misturou os eixos), e depois que a placa para a posição continua derivando em linha reta.

A comparação mostra na prática a limitação da integração dupla: qualquer resto de velocidade vira uma deriva de posição que cresce com o tempo. Movimentos curtos e bruscos dão um resultado coerente, enquanto movimentos lentos ficam abaixo do que o método consegue medir e capturas longas acumulam erro. Uma melhoria simples seria zerar a velocidade sempre que a placa ficar parada por alguns instantes.
