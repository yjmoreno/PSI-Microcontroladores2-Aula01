# Prompt usado na atividade Race Condition

Estou usando a placa FRDM-KL25Z com Zephyr 4.2 no PlatformIO. Preciso de um código simples em C que mostre uma race condition (condição de corrida) quando duas threads usam a mesma variável sem proteção.

Quero usar como exemplo um estacionamento com duas cancelas:
- uma thread é a cancela de entrada e soma 1 no número de carros dentro
- outra thread é a cancela de saída e tira 1
- cada cancela passa 1000 carros, então no final tem que dar 0

Coloca as duas threads com a mesma prioridade e liga o time slicing com 1 ms, pra trocar de thread no meio da conta. Na hora de atualizar o contador, lê o valor, espera um pouco com k_busy_wait() (como se fosse o tempo do sensor) e só depois grava, pra o erro aparecer.

No final o main mostra no log quantos carros ficaram dentro. Se der 0 acende o LED verde, se der errado acende o vermelho. Coloca também um #define PROTEGER que, se for 1, usa um mutex, pra eu comparar os dois casos.

Me manda o prj.conf também e explica rapidinho por que o erro acontece.
