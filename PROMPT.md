# Prompt usado na atividade Race Condition

Estou usando a placa NXP FRDM-KL25Z com Zephyr 4.2 e PlatformIO. Quero um código em C simples, de nível de aluno de graduação, que mostre na prática uma condição de corrida (race condition) causada pelo uso de um recurso compartilhado sem sincronização.

Use como exemplo real um estacionamento com duas cancelas. Uma thread representa a cancela de entrada e soma 1 a uma variável global com o número de carros dentro do estacionamento, e a outra thread representa a cancela de saída e subtrai 1 dessa mesma variável. Cada cancela deve processar 1000 carros, então o número de carros dentro deve terminar em zero. As duas threads devem ter a mesma prioridade e o time slicing deve estar ligado com fatia de 1 ms, para que o escalonador troque de thread no meio da atualização. Na atualização do contador, faça a leitura da variável, espere um pouco com k_busy_wait() simulando o tempo de processar o sensor da cancela e só depois grave o novo valor, para deixar a janela da corrida visível.

No final, o main deve esperar as duas threads terminarem e mostrar com Logging quantos carros ficaram dentro, comparando com o valor esperado. Acenda o LED verde da placa se o resultado estiver certo e o vermelho se estiver errado. Inclua uma macro PROTEGER que, quando vale 1, protege a atualização do contador com um mutex, para eu comparar os dois casos.

Passe também o prj.conf necessário e explique brevemente por que o erro acontece.
