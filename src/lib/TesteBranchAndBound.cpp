#include <solvers/DispatchSolver.h>
#include <JobShopInstance.h>
#include <Rules.h>

#include <algorithm>
#include <climits>
#include <queue>
#include <vector>
#include <map>

struct Node {
  std::vector<int>
      next_op_index; // Próximo índice de operação a ser agendada para cada job
  std::vector<int> machine_free_time; // Tempo em que cada máquina estará livre
  std::vector<int> job_completion_time; // Tempo de conclusão da última operação
                                        // agendada de cada job
  int current_makespan; // Makespan atual (para o Upper Bound)
  int lower_bound;      // Limite Inferior (Chave de prioridade)

  // Agendamento parcial: Lista de operações já sequenciadas
  std::vector<Operation> partial_schedule;

  auto operator<=>(const Node &other) const {
    return lower_bound <=> other.lower_bound;
  }
};

struct OperationDataToSingloMachine {
  int job_id;
  int duration;     // p_j (Processing Time)
  int release_time; // r_j (Release Time / Ready Time)
  int due_date;     // d_j (Due Date - Ignorado no cálculo C_max, mas mantido)
};

// Comparador para operações NÃO liberadas: Ordena por r_j (menor r_j no topo)
struct ReleaseTimeComparator {
  bool operator()(const OperationDataToSingloMachine &a,
                  const OperationDataToSingloMachine &b) const {
    return a.release_time > b.release_time;
  }
};

// Comparador para operações JÁ liberadas: Ordena por p_j (menor duração/SPT no
// topo) Esta é a regra ótima para 1 | r_j | C_max
struct ProcessingTimeComparator {
  bool operator()(const OperationDataToSingloMachine &a,
                  const OperationDataToSingloMachine &b) const {
    if (a.duration != b.duration) {
      return a.duration > b.duration; // SPT: Menor p_j no topo
    }
    return a.release_time > b.release_time; // Desempate: Menor r_j
  }
};

/**
 * Computa o tempo de início mais cedo possível (r_ij) para uma operação.
 * Equivalente ao comprimento do caminho mais longo da origem U para o nó da
 * operação (j, i).
 *
 * @param node O estado parcial atual (vértices já alcançados/sequenciados).
 * @param op A operação (i,j) para a qual o r_ij está sendo calculado.
 * @return O Tempo de início (r_ij) da operação.
 */
int calculate_longest_path_U_to_O(const Node &node, const Operation &op) {
  // Restrição de Precedência Tecnológica (Arcos Conjuntivos)
  // A operação op só pode começar DEPOIS que a sua predecessora no mesmo Job
  // terminar.
  int predecessor_job_completion_time = 0;
  if (op.position_in_job > 0) {
    // Usa o tempo de conclusão registrado do Job.
    // Se a Op (j, i) não é a primeira do Job, este valor é > 0.
    predecessor_job_completion_time = node.job_completion_time[op.job_id];
  }
  // Restrição de Recurso (Arcos Disjuntivos JÁ SELECIONADOS)
  // A operação op só pode começar DEPOIS que a Máquina i estiver livre.
  int machine_free_time = node.machine_free_time[op.machine_id];

  // O Caminho Mais Longo (max)
  // No grafo, é o máximo dos comprimentos dos caminhos que chegam em O_ij.
  int r_ij = std::max(predecessor_job_completion_time, machine_free_time);

  return r_ij;
}

/**
 * SOLUÇÃO ÓTIMA para 1 | r_j | C_max (Garante o Lower Bound Válido)
 *
 * @param machine_ops Vetor de operações com r_j e p_j.
 * @return O Makespan (tempo de conclusão total) ótimo para o problema relaxado.
 */
int solve_single_machine_makespan_for_Lmax(
    const std::vector<OperationDataToSingloMachine> &machine_ops) {
  if (machine_ops.empty())
    return 0;

  // 1. Fila de não-liberados (ordenada pelo r_j mais cedo)
  std::priority_queue<OperationDataToSingloMachine,
                      std::vector<OperationDataToSingloMachine>,
                      ReleaseTimeComparator>
      not_released_ops;
  for (const auto &op : machine_ops) {
    not_released_ops.push(op);
  }

  // 2. Fila de prontos (ordenada pelo p_j mais curto - Regra SPT)
  std::priority_queue<OperationDataToSingloMachine,
                      std::vector<OperationDataToSingloMachine>,
                      ProcessingTimeComparator>
      ready_ops;

  int current_time = 0;
  int max_completion_time = 0; // O Makespan (C_max)

  while (!not_released_ops.empty() || !ready_ops.empty()) {
    // --- Passo A: Liberar Operações ---
    // Move todas as operações prontas (r_j <= current_time) para o conjunto
    // READY (SPT)
    while (!not_released_ops.empty() &&
           not_released_ops.top().release_time <= current_time) {
      ready_ops.push(not_released_ops.top());
      not_released_ops.pop();
    }

    // --- Passo B: Seleção da Operação ---
    if (!ready_ops.empty()) {
      // Regra Ótima: Escolhe a operação liberada com o menor Tempo de
      // Processamento (SPT)
      OperationDataToSingloMachine selected_op = ready_ops.top();
      ready_ops.pop();
      // Tempo de início da operação:
      // max(Tempo atual da Máquina, Tempo de Liberação da Operação)
      int start_time = std::max(current_time, selected_op.release_time);

      // Tempo de conclusão
      int completion_time = start_time + selected_op.duration;
      // Atualiza o tempo atual da máquina
      current_time = completion_time;
      max_completion_time = std::max(max_completion_time, completion_time);
    } else if (!not_released_ops.empty()) {
      // Se não há operações prontas, avança o tempo para a próxima liberação
      current_time = not_released_ops.top().release_time;
    }
  }

  // Retorna o Makespan Ótimo para 1 | r_j | C_max
  return max_completion_time;
}

int calculate_lower_bound(const Node &node, const JobShopInstance &instance) {
  int max_lb = node.current_makespan;

  // Estrutura para agrupar as operações elegíveis por máquina
  std::map<int, std::vector<OperationDataToSingloMachine>> ops_by_machine;

  // --- Parte 1: Limite Inferior Baseado em Job (Job-Based LB) ---
  for (int j = 0; j < instance.n_jobs; ++j) {
    // Tempo restante do Job (Caminho Mais Longo O_ij -> V, onde O_ij é a
    // próxima op)
    int remaining_time_path_to_V = 0;

    for (int k = node.next_op_index[j]; k < instance.jobs[j].size(); ++k) {
      const Operation &op = instance.jobs[j][k];
      remaining_time_path_to_V += op.duration;
    }

    // LB = max(Makespan Atual, C_job = Conclusão Passada + Tempo Restante)
    max_lb = std::max(max_lb,
                      node.job_completion_time[j] + remaining_time_path_to_V);

    // Prepara os dados para o Limite Inferior Baseado em Máquina
    if (node.next_op_index[j] < instance.jobs[j].size()) {
      const Operation &op = instance.jobs[j][node.next_op_index[j]];

      // r_ij é o Caminho Mais Longo U -> O_ij
      int r_ij = calculate_longest_path_U_to_O(node, op);

      // p_ij é a duração
      int p_ij = op.duration;

      // Coleta a operação para a máquina
      ops_by_machine[op.machine_id].push_back({op.job_id, p_ij, r_ij, 0});
    }
  }

  // --- Parte 2: Limite Inferior Baseado em Máquina (Machine-Based LB) ---

  for (int m = 0; m < instance.n_machines; ++m) {
    if (ops_by_machine.count(m)) {
      const auto &remaining_ops = ops_by_machine.at(m);

      // Resolve o problema ótimo 1 | r_j | C_max (SPT-Gulosa)
      int lb_machine = solve_single_machine_makespan_for_Lmax(remaining_ops);

      // Atualiza o LB global do nó
      max_lb = std::max(max_lb, lb_machine);
    }
  }

  return max_lb;
}

// Simplificado
// int calculate_lower_bound(const Node& node, JobShopInstance instance) {
//   int lb = node.current_makespan;

//   for (int j = 0; j < instance.n_jobs; ++j) {
//       int remaining_time = 0;
//       for (int k = node.next_op_index[j]; k < instance.jobs[j].size(); ++k) {
//           remaining_time += instance.jobs[j][k].duration;
//       }
//       lb = std::max(lb, node.job_completion_time[j] + remaining_time);
//   }
//   return lb;
// }

int solve_branch_and_bound(JobShopInstance instance) {
  // Inicializando o Makespan com a heurística com regra de dispache SPT
  Rules::Rule rule = Rules::shortest_processing_time;
  DispatchSolver solver(instance, rule);
  int upper_bound = solver.solve().makespan();

  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> queue;

  // 1. Inicializar nó raiz
  Node root;
  root.next_op_index.assign(instance.n_jobs, 0);
  root.machine_free_time.assign(instance.n_machines, 0);
  root.job_completion_time.assign(instance.n_jobs, 0);
  root.current_makespan = 0;
  root.lower_bound = calculate_lower_bound(root, instance);
  queue.push(root);

  while (!queue.empty()) {
    Node current_node = queue.top();
    queue.pop();

    // Poda (Pruning)
    if (current_node.lower_bound >= upper_bound)
      continue;

    // Verifica se todos os jobs já terminaram
    bool all_jobs_finished = true;
    for (int job = 0; job < instance.n_jobs; job++) {
      if (current_node.next_op_index[job] < instance.jobs[job].size()) {
        all_jobs_finished = false;
        break;
      }
    }

    if (all_jobs_finished) {
      // Atualiza o Makespan ótimo
      upper_bound = std::min(upper_bound, current_node.current_makespan);
      continue;
    }

    // Ramificação

    // Identificar as operações elegíveis (aquelas que são as próximas no seu
    // job)
    std::vector<Operation> eligible_ops;
    for (int job = 0; job < instance.n_jobs; ++job) {
      if (current_node.next_op_index[job] < instance.jobs[job].size()) {
        eligible_ops.push_back(
            instance.jobs[job][current_node.next_op_index[job]]);
      }
    }

    // 2. Estratégia de Branching: Foca na ramificação na máquina da operação
    // elegível que, se agendada imediatamente, terminaria no tempo mais curto
    int min_completion_time = INT_MAX;
    int critical_machine_id = -1;

    for (const auto &op : eligible_ops) {
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

    // 3. Ramificação Focada: Cria um nó filho APENAS para as operações
    // elegíveis
    //    que utilizam a máquina crítica i*.
    for (const auto &op_to_schedule : eligible_ops) {
      // Ramificar apenas se a operação usar a máquina i* encontrada no Passo 2.
      if (op_to_schedule.machine_id == critical_machine_id) {

        Node new_node = current_node;

        // Determinar o tempo de início (r_ij) para esta operação
        int start_time =
            std::max(new_node.machine_free_time[op_to_schedule.machine_id],
                     new_node.job_completion_time[op_to_schedule.job_id]);

        // Agendamento (Completion Time = start_time + p_ij)
        int completion_time = start_time + op_to_schedule.duration;

        // Atualizar o estado (criação do nó filho)
        new_node.machine_free_time[op_to_schedule.machine_id] = completion_time;
        new_node.job_completion_time[op_to_schedule.job_id] = completion_time;
        new_node.current_makespan =
            std::max(new_node.current_makespan, completion_time);
        new_node.next_op_index[op_to_schedule.job_id]++;

        // Rastrear o agendamento
        // TODO: Acho que não precisa dessa parte
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
