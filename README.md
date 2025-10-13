# Job Shop Scheduling Problem
**Autores:**
- Lucas Apolonio de Amorim ([@lucasaamorim](https://github.com/lucasaamorim))
- Moisés Ferreira de Lima ([@moisesferreira123](https://github.com/moisesferreira123))

## Conteúdos

1. [Introdução](#introdução)
2. [Abordagem Heurística](#abordagem-heurística-com-regras-de-despacho)
3. [Compilação e Execução](#compilação-e-execução)

## Introdução
O Job Shop Scheduling Problem (JSSP) é um problema de otimização combinatória que consiste em encontrar uma sequência de tarefas para um conjunto de máquinas que minimize o tempo total de conclusão (makespan). O problema é conhecido por ser NP-difícil, o que significa que não existem algoritmos conhecidos que possam resolvê-lo otimamente em tempo polinomial para todas as instâncias.

Este projeto implementa uma solução heurística para o JSSP baseada em regras de despacho (dispatching rules).

## Abordagem Heurística com Regras de Despacho
A solução implementada utiliza um mecanismo de despacho para construir uma solução de forma construtiva. Em cada passo, uma lista de operações "disponíveis" (cujas predecessoras já foram agendadas) é gerada. Uma **regra de despacho** é então utilizada para selecionar qual operação será a próxima a ser agendada.

Este projeto implementa as seguintes regras:
-   **Shortest Processing Time (SPT):** Seleciona a operação com o menor tempo de processamento.
-   **Longest Processing Time (LPT):** Seleciona a operação com o maior tempo de processamento.
-   **Most Work Remaining (MWR):** Prioriza a tarefa (job) que possui a maior soma de tempos de processamento restantes.
-   **First Come, First Served (FCFS):** Seleciona a operação que pertence à tarefa que está há mais tempo no sistema.
-   **Random:** Seleciona uma operação aleatória da lista de candidatas.

## Compilação e Execução

### Compilação
Do diretório principal do projeto, execute os seguintes comandos para compilar:
```
# Cria e configura o diretório de build
cmake -B build

# Compila o projeto
cmake --build build
```
<!-- CMakeLists.txt bugado, não deu pra compilar
Opcionalmente, para compilar apenas o gerador de instâncias de teste:
```
cmake --build build --target testgen
```
-->
### Execução
O programa aceita um arquivo de instância ou um diretório como argumento. Se um diretório for fornecido, ele será percorrido recursivamente, e todas as instâncias encontradas serão resolvidas.

**Sintaxe:**
```
./build/Job-Shop [opções] <caminho_para_instância_ou_diretório>
```

**Opções:**
-   `-r, --rule <NOME_DA_REGRA>`: Escolhe a regra de despacho a ser utilizada.
-   `-h, --help`: Exibe a mensagem de ajuda com as regras disponíveis.

**Exemplo de uso:**
```
# Executa para uma única instância com a regra padrão (shortest_processing_time)
./build/Job-Shop tests/instance_sets/ft/ft06.txt

# Executa para todas as instâncias no diretório 'la' com a regra 'most_work_remaining'
./build/Job-Shop -r most_work_remaining tests/instance_sets/la
```
Os resultados são impressos na tela e salvos em um arquivo CSV (ex: `most_work_remaining.csv`) no diretório raiz do projeto.

Para rodar um teste pré-configurado, use o target `run_test`:
```
cmake --build build --target run_test
```
