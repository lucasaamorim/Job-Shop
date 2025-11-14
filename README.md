# Job Shop Scheduling Problem

O Job Shop Scheduling Problem (JSSP) é um problema de otimização combinatória que consiste em encontrar uma sequência de tarefas para um conjunto de máquinas que minimize o tempo total de conclusão (makespan). O problema é conhecido por ser NP-difícil, o que significa que não existem algoritmos conhecidos que possam resolvê-lo otimamente em tempo polinomial para todas as instâncias.

## Autores
- Lucas Apolonio de Amorim ([@lucasaamorim](https://github.com/lucasaamorim))
- Moisés Ferreira de Lima ([@moisesferreira123](https://github.com/moisesferreira123))

## Conteúdos

1. [Introdução](#introdução)
2. [Abordagens de Solução](#abordagens-de-solução)
3. [Compilação e Execução](#compilação-e-execução)
4. [Saída (Output)](#saída-output)

## Introdução
Este projeto implementa soluções para o JSSP, oferecendo abordagens heurísticas e otimizadoras para encontrar o makespan mínimo.

## Abordagens de Solução

O programa suporta dois algoritmos principais, selecionáveis pela opção `--algorithm` (`-a`): `priority_dispatch` (heurística) e `branch_and_bound` (otimizadora).

### 1. Abordagem Heurística com Regras de Despacho (`priority_dispatch`)

A solução utiliza um mecanismo de despacho para construir um agendamento de forma construtiva. Em cada passo, uma **regra de despacho** é usada para selecionar qual operação disponível será a próxima a ser agendada.

Regras de Despacho Implementadas:
-   **Shortest Processing Time (SPT):** Seleciona a operação com o menor tempo de processamento.
-   **Longest Processing Time (LPT):** Seleciona a operação com o maior tempo de processamento.
-   **Most Work Remaining (MWR):** Prioriza a tarefa (job) que possui a maior soma de tempos de processamento restantes.
-   **First Come, First Served (FCFS):** Seleciona a operação que pertence à tarefa que está há mais tempo no sistema.
-   **Random:** Seleciona uma operação aleatória da lista de candidatas.

### 2. Abordagem Otimizadora (Branch and Bound) (`branch_and_bound`)

O algoritmo Branch and Bound (B&B) realiza uma busca *Best-First* para encontrar a solução ótima, minimizando o makespan.

-   **Upper Bound Inicial:** Um limite superior inicial (`upper_bound`) é estabelecido executando múltiplas regras de despacho heurísticas no início, permitindo uma poda (pruning) mais eficiente da árvore de busca.
-   **Lower Bound:** O limite inferior (`lower_bound`) de cada nó é calculado como o máximo entre:
    1.  O makespan atual do nó.
    2.  O **Lower Bound por Job** (tempo de conclusão do job + trabalho restante).
    3.  O **Lower Bound por Máquina** (resolvido como um problema $1|r_j|C_{max}$ para as operações restantes na máquina).
-   **Ramificação:** A ramificação ocorre nas operações elegíveis na **máquina crítica** (aquela que permite a conclusão de uma operação o mais cedo possível no nó atual).

## Compilação e Execução

O projeto utiliza o CMake para gerenciar a construção.

### Compilação
Do diretório principal do projeto, execute os seguintes comandos:
```bash
# Cria e configura o diretório de build
cmake -B build

# Compila o projeto (cria o executável ./build/Job-Shop)
cmake --build build
```

### Execução
O executável `Job-Shop` pode resolver uma única instância ou percorrer um diretório recursivamente, salvando os resultados em um arquivo CSV.

**Sintaxe:**
```bash
./build/Job-Shop [opções] <caminho_para_instância_ou_diretório>
```

**Opções:**
-   `-a, --algorithm <NOME_DO_ALGORITMO>`: Escolhe o algoritmo (`priority_dispatch` ou `branch_and_bound`). Padrão: `priority_dispatch`.
-   `-r, --rule <NOME_DA_REGRA>`: (Apenas para `priority_dispatch`) Escolhe a regra de despacho (e.g., `shortest_processing_time`). Padrão: `shortest_processing_time`.
-   `-t, --timeout <SEGUNDOS>`: Define um limite de tempo em segundos para a execução. Padrão: sem limite.
-   `-h, --help`: Exibe a mensagem de ajuda.

**Exemplo de uso (Heurística):**
```bash
# Executa para todas as instâncias no diretório 'la' com a regra 'most_work_remaining'
./build/Job-Shop -r most_work_remaining tests/instance_sets/la
```

**Exemplo de uso (Otimizadora):**
```bash
# Executa Branch and Bound para uma única instância com timeout de 30 segundos
./build/Job-Shop -a branch_and_bound -t 30 tests/instance_sets/ft/ft10.txt
```

## Saída (Output)

Os resultados (Nome da Instância, Tempo de Execução, Makespan e Status de Timeout) são exibidos na tela e salvos em um arquivo CSV no diretório raiz do projeto.

O nome do arquivo CSV segue o padrão:
-   `priority_dispatch_<NOME_DA_REGRA>.csv` (para a heurística)
-   `branch_and_bound.csv` (para o B&B)
