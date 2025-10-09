# Job Shop Scheduling Problem
**Autores:**
- Lucas Apolonio de Amorim ([@lucasaamorim](https://github.com/lucasaamorim))
- Moisés Ferreira de Lima ([@moisesferreira123](https://github.com/moisesferreira123))

## Conteúdos

1. [Introdução](#introdução)
2. [Heurística de Shifting Bottleneck](#heurística-de-shifting-bottleneck)
3. [Compilação e Execução](#compilação-e-execução)

## Introdução
O Job Shop Scheduling Problem é um problema de otimização combinatória que consiste em encontrar uma sequência de tarefas para uma máquina que minimize o tempo de conclusão do conjunto de tarefas. O problema é conhecido por ser NP-difícil, o que significa que não há algoritmo polinomial que possa resolver todas as instâncias do problema em tempo polinomial.

Este projeto implementa uma solução para o Job Shop Scheduling Problem usando a heurística de Shifting Bottleneck.

## Heurística de Shifting Bottleneck
A heurística de Shifting Bottleneck consiste em identificar possíveis "gargalos" e priorizar eles na construção do nosso agendamento. Nessa heurística cada máquina é resolvida como uma instância de agendamento para somente uma máquina, que é um problema para o qual existem algoritmos eficientes. E com base no tempo total de conclusão para cada máquina, denominamos de gargalo a máquina com o maior tempo total de conclusão.

## Compilação e Execução
Do diretório principal do projeto, compilar com:
```bash
cmake -B build

cmake --build build
```

Para executar:
```bash
./build/job_shop
```
