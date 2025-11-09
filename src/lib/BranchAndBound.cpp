#include <Operation.h>
#include <JobShopInstance.h>

#include <vector>
#include <queue>

struct Node {
  std::vector<int> next_op_index;  // Próximo índice de operação a ser agendada para cada job
  std::vector<int> machine_free_time; // Tempo em que cada máquina estará livre
  std::vector<int> job_completion_time; // Tempo de conclusão da última operação agendada de cada job
  int current_makespan;        // Makespan atual (para o Upper Bound)
  int lower_bound;             // Limite Inferior (Chave de prioridade)

  // Agendamento parcial: Lista de operações já sequenciadas
  std::vector<Operation> partial_schedule;
};

int solve_branch_and_bound(Node current_node) {
  
}