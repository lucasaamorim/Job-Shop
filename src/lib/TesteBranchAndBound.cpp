#include <Operation.h>
#include <JobShopInstance.h>
#include <Solver.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <queue>
#include <climits>
#include <numeric>

struct Node {
  std::vector<int> next_op_index;  // Próximo índice de operação a ser agendada para cada job
  std::vector<int> machine_free_time; // Tempo em que cada máquina estará livre
  std::vector<int> job_completion_time; // Tempo de conclusão da última operação agendada de cada job
  int current_makespan;        // Makespan atual (para o Upper Bound)
  int lower_bound;             // Limite Inferior (Chave de prioridade)

  // Agendamento parcial: Lista de operações já sequenciadas
  std::vector<Operation> partial_schedule;
};

// Simplificado
int calculate_lower_bound(const Node& node, JobShopInstance instance) {
  int lb = node.current_makespan;

  for (int j = 0; j < instance.n_jobs; ++j) {
      int remaining_time = 0;
      for (int k = node.next_op_index[j]; k < instance.jobs[j].size(); ++k) {
          remaining_time += instance.jobs[j][k].duration;
      }
      lb = std::max(lb, node.job_completion_time[j] + remaining_time);
  }
  return lb;
}

int solve_branch_and_bound(JobShopInstance instance) {
  // Inicializando o Makespan com a heurística com regra de dispache SPT
  Rules::Rule rule = Rules::shortest_processing_time;
  Solver solver(instance, rule);
  int upper_bound = solver.solve().makespan(); 

  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> queue;

  // Inicializar nó raiz
  Node root;
  root.next_op_index.assign(instance.n_jobs, 0);
  root.machine_free_time.assign(instance.n_machines, 0);
  root.job_completion_time.assign(instance.n_jobs, 0);
  root.current_makespan = 0;
  root.lower_bound = calculate_lower_bound(root, instance);
  queue.push(root);

  while(!queue.empty()) {
    Node current_node = queue.top();
    queue.pop();

    // Poda (Pruning)
    if(current_node.lower_bound >= upper_bound) continue;

    // Verifica se todos os jobs já terminaram
    bool all_jobs_finished = true;
    for(int job=0; job<instance.n_jobs; job++) {
      if(current_node.next_op_index[job] < instance.jobs[job].size()) {
        all_jobs_finished = false;
        break;
      }
    }
    
    if(all_jobs_finished) {
      // Atualiza o Makespan ótimo
      upper_bound = std::min(upper_bound, current_node.current_makespan);
      continue;
    }

    // Ramificação

    // Identificar as operações elegíveis (aquelas que são as próximas no seu job)
    std::vector<Operation> eligible_ops;
    for (int job = 0; job < instance.n_jobs; ++job) {
      if (current_node.next_op_index[job] < instance.jobs[job].size()) {
        eligible_ops.push_back(instance.jobs[job][current_node.next_op_index[job]]);
      }
    }

    // Estratégia de Branching: Foca na ramificação na máquina da operação elegível que,
    // se agendada imediatamente, terminaria no tempo mais curto
    int min_completion_time = INT_MAX;
    int critical_machine_id = -1;

    for (const auto& op : eligible_ops) {
      int machine_id = op.machine_id;
      int job_id = op.job_id;
      int p_ij = op.duration;
      
      // r_ij (Release Time/Ready Time): Tempo que a operação PODE começar.
      // É o máximo entre:
      // a) Tempo de conclusão da operação anterior no Job 
      // b) Tempo de liberação da máquina 
      int r_ij = std::max(current_node.job_completion_time[job_id], 
                     current_node.machine_free_time[machine_id]);
      
      // Cálculo da métrica: C_ij = r_ij + p_ij
      int c_ij = r_ij + p_ij;
      
      // Encontrar o mínimo e identificar a máquina i*
      if (c_ij < min_completion_time) {
        min_completion_time = c_ij;
        critical_machine_id = machine_id;
      }
    }

    for (const auto& op_to_schedule : eligible_ops) {
      // Ramificar apenas se a operação usar a máquina i* encontrada no Passo 2.
      if (op_to_schedule.machine_id == critical_machine_id) {
          
        Node new_node = current_node;

        // Determinar o tempo de início (r_ij) para esta operação
        int start_time = std::max(new_node.machine_free_time[op_to_schedule.machine_id], 
                             new_node.job_completion_time[op_to_schedule.job_id]);
        
        // Agendamento (Completion Time = start_time + p_ij)
        int completion_time = start_time + op_to_schedule.duration;
        
        // Atualizar o estado (criação do nó filho)
        new_node.machine_free_time[op_to_schedule.machine_id] = completion_time;
        new_node.job_completion_time[op_to_schedule.job_id] = completion_time;
        new_node.current_makespan = std::max(new_node.current_makespan, completion_time);
        new_node.next_op_index[op_to_schedule.job_id]++;
        
        // Rastrear o agendamento
        Operation scheduled_op = op_to_schedule;
        new_node.partial_schedule.push_back(scheduled_op);
        
        // Calcular novo Limite Inferior e Adicionar à Fila
        new_node.lower_bound = calculate_lower_bound(new_node, instance);
        if (new_node.lower_bound < upper_bound) {
            queue.push(new_node);
        }
      }
    }
  }
  return upper_bound;
}