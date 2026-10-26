# Atividade — Race Condition

Placa FRDM-KL25Z com Zephyr 4.2, gravada pelo PlatformIO. Tudo desta atividade está na branch `race-condition`: o prompt em [`PROMPT.md`](PROMPT.md), o código gerado em [`src/main.c`](src/main.c), a configuração em [`zephyr/prj.conf`](zephyr/prj.conf) e os logs dos testes na pasta [`logs/`](logs/).

## O exemplo escolhido

Escolhi um estacionamento com duas cancelas, uma de entrada e uma de saída, que atualizam o mesmo contador de carros. É uma situação bem comum em sistemas embarcados: duas tarefas independentes, cada uma ligada a um sensor, mexendo na mesma variável global. Se o contador estiver errado, o painel da entrada pode mostrar que há vagas quando o estacionamento está lotado, ou o contrário.

No código, cada cancela é uma thread e processa 1000 carros. A de entrada soma 1 ao contador e a de saída subtrai 1. Como entram e saem 1000 carros, o contador deveria terminar em zero.

## O prompt

Este foi o prompt que usei na IA:

> Estou usando a placa FRDM-KL25Z com Zephyr 4.2 no PlatformIO. Preciso de um código simples em C que mostre uma race condition (condição de corrida) quando duas threads usam a mesma variável sem proteção.
>
> Quero usar como exemplo um estacionamento com duas cancelas:
> - uma thread é a cancela de entrada e soma 1 no número de carros dentro
> - outra thread é a cancela de saída e tira 1
> - cada cancela passa 1000 carros, então no final tem que dar 0
>
> Coloca as duas threads com a mesma prioridade e liga o time slicing com 1 ms, pra trocar de thread no meio da conta. Na hora de atualizar o contador, lê o valor, espera um pouco com k_busy_wait() (como se fosse o tempo do sensor) e só depois grava, pra o erro aparecer.
>
> No final o main mostra no log quantos carros ficaram dentro. Se der 0 acende o LED verde, se der errado acende o vermelho. Coloca também um #define PROTEGER que, se for 1, usa um mutex, pra eu comparar os dois casos.
>
> Me manda o prj.conf também e explica rapidinho por que o erro acontece.

## O código gerado

O ponto central do código é a função que atualiza o contador. Ela faz a operação em três passos separados, do jeito que um programa "ingênuo" faria: lê o valor, processa e grava.

```c
static void atualiza_contador(int delta)
{
#if PROTEGER
    k_mutex_lock(&trava, K_FOREVER);
#endif
    int copia = carros_dentro;      /* 1. le o valor atual */
    k_busy_wait(50);                /* 2. tempo de processar o sensor (50 us) */
    carros_dentro = copia + delta;  /* 3. grava o novo valor */
#if PROTEGER
    k_mutex_unlock(&trava);
#endif
}
```

As duas threads têm a mesma prioridade (7), e no `prj.conf` o *time slicing* está com fatia de 1 ms (`CONFIG_TIMESLICE_SIZE=1`). Assim o escalonador alterna entre as cancelas a cada 1 ms, sem que nenhuma delas peça. O `main` espera as duas terminarem usando um semáforo, mostra o resultado no log e acende o LED.

## Resultados na placa

Rodei o programa três vezes sem proteção e três vezes com o mutex, apertando o RESET da placa com o monitor serial aberto.

| | Sem proteção (`PROTEGER 0`) | Com mutex (`PROTEGER 1`) |
|---|---|---|
| Carros dentro no final (esperado: 0) | **−983** | **0** |
| Resultado nas 3 execuções | −983, −983, −983 | 0, 0, 0 |
| LED | vermelho | verde |
| Tempo total | 195 ms | 285 ms |

![LEDs da placa](docs/leds_race.jpg)

*À esquerda, a versão sem proteção terminando com o LED vermelho (contagem errada). À direita, a versão com mutex terminando com o LED verde (contagem certa).*

## Por que deu −983

O problema está no intervalo entre ler e gravar o contador. Cada atualização leva uns 50 µs, e quase todo esse tempo é a espera do passo 2. Então, quando a fatia de 1 ms acaba, a troca de thread quase sempre acontece justamente nesse intervalo: a cancela já leu o valor, mas ainda não gravou.

Quando essa cancela volta a executar, ela grava a cópia antiga mais 1 (ou menos 1). Com isso, ela apaga tudo o que a outra cancela fez durante a fatia dela. A tabela abaixo mostra o começo da execução, de forma simplificada:

| Momento | Cancela de entrada | Cancela de saída | `carros_dentro` |
|---|---|---|---|
| fatia 1 | conta 17 carros e para no meio do 18º, com a cópia = 17 | esperando | 17 |
| fatia 2 | esperando | lê 17, conta uns 18 carros saindo e para no meio de mais um | ≈ 0 |
| fatia 3 | grava 17 + 1 = 18, **apagando as saídas da fatia 2** | esperando | 18 |
| fatia 4 | esperando | grava a cópia antiga − 1, **apagando as entradas da fatia 3** | ≈ −1 |

Na prática, cada cancela passa a "enxergar" só as suas próprias contas, e o valor final é o da cancela que gravou por último. A entrada começou primeiro e terminou primeiro, então quem gravou por último foi a saída. Ela partiu do 17 que leu no início e subtraiu os seus 1000 carros: 17 − 1000 = **−983**. Para conferir, fiz uma simulação simples desse mecanismo no computador com os mesmos tempos, e ela deu −982.

O resultado se repetiu nas três execuções porque a placa sempre começa do mesmo jeito: o boot leva o mesmo tempo, então as trocas de thread caem sempre nos mesmos pontos do código. Num sistema real, com sensores de verdade gerando eventos em instantes aleatórios, o erro mudaria a cada vez, o que deixa esse tipo de bug ainda mais difícil de encontrar.

Isso tem a ver com o que o livro-texto chama de seção crítica (seção 2.7, p. 105–106). O exemplo do livro é uma variável de 32 bits lida num barramento de 8 bits: se uma interrupção alterar a variável no meio da leitura, a tarefa fica com parte do valor antigo e parte do novo. Aqui é a mesma ideia, só que a "leitura" e a "escrita" estão separadas por mais tempo, então o problema aparece sempre.

## Por que o mutex resolve e quanto custa

Com `PROTEGER 1`, os três passos ficam entre `k_mutex_lock()` e `k_mutex_unlock()`. A troca de thread continua acontecendo a cada 1 ms, mas agora, se a outra cancela tentar atualizar o contador enquanto a trava está fechada, ela fica bloqueada até a primeira terminar. Ler, processar e gravar passou a ser uma operação que ninguém interrompe pela metade, e por isso o resultado foi 0 nas três execuções.

A proteção tem um custo. O tempo total subiu de 195 ms para 285 ms, ou seja, uns 45 µs a mais em cada uma das 2000 atualizações. Esse tempo vem das chamadas de `lock` e `unlock` em toda atualização e das trocas de thread extras quando uma cancela encontra a trava fechada e precisa esperar. Para este exemplo o custo é pequeno, mas mostra por que a seção crítica deve ser a menor possível.

O livro cita três jeitos de proteger um recurso compartilhado (p. 106–107): desabilitar as interrupções, desabilitar o escalonador ou usar semáforos e mutex. O mutex é o mais recomendado entre threads, porque não aumenta a latência das interrupções nem trava as outras tarefas que não usam o recurso.
