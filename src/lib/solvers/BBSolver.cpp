#include <JobShopInstance.h>
#include <Rules.h>
#include <solvers/BBSolver.h>
#include <solvers/DispatchSolver.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

// Anonymous namespace for internal B&B implementation details
namespace {

/**
 * @brief Represents a node in the Branch & Bound search tree.
 */
struct Node {
  std::vector<int> next_op_index;     // Next op index for each job
  std::vector<int> machine_free_time; // Time each machine is free
  std::vector<int>
      job_completion_time; // Completion time of last op for each job
  int current_makespan;    // Makespan of the partial schedule
  int lower_bound;         // Lower bound (priority key)

  /**
   * @brief Stores the actual scheduled operations to reconstruct the
   * Schedule.
   */
  std::shared_ptr<const Node> parent;

  std::optional<ScheduledOperation> scheduled_op;

  /**
   * @brief Comparator for the priority queue (min-heap).
   */
  bool operator>(const Node &other) const {
    return lower_bound > other.lower_bound;
  }
};

struct NodeComparator {
  bool operator()(const std::shared_ptr<const Node> &a,
                  const std::shared_ptr<const Node> &b) const {
    return a->lower_bound > b->lower_bound;
  }
};

// --- Helpers for Lower Bound Calculation ---

/**
 * @brief Data for solving the 1 | r_j | C_max relaxation.
 */
struct OperationDataToSingloMachine {
  int job_id;
  int duration;     // p_j (Processing Time)
  int release_time; // r_j (Release Time / Ready Time)
  int due_date;     // d_j (Due Date - Ignored)
};

/**
 * @brief Comparator for operations NOT released: Ordena por r_j.
 */
struct ReleaseTimeComparator {
  bool operator()(const OperationDataToSingloMachine &a,
                  const OperationDataToSingloMachine &b) const {
    return a.release_time > b.release_time;
  }
};

/**
 * @brief Comparator for operations JÁ released: Ordena por p_j (SPT).
 */
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
 * Computes the earliest possible start time (r_ij) for an operation.
 * (Longest path from U to O_ij)
 */
int calculate_longest_path_U_to_O(const Node &node, const Operation &op) {
  // Precedence constraint (job)
  int predecessor_job_completion_time = 0;
  if (op.position_in_job > 0) {
    predecessor_job_completion_time = node.job_completion_time[op.job_id];
  }
  // Resource constraint (machine)
  int machine_free_time = node.machine_free_time[op.machine_id];

  // r_ij is the max of the two
  return std::max(predecessor_job_completion_time, machine_free_time);
}

/**
 * Solves the 1 | r_j | C_max problem optimally using SPT rule.
 * Returns the makespan (C_max).
 */
int solve_single_machine_makespan_for_Lmax(
    const std::vector<OperationDataToSingloMachine> &machine_ops) {
  if (machine_ops.empty())
    return 0;

  std::priority_queue<OperationDataToSingloMachine,
                      std::vector<OperationDataToSingloMachine>,
                      ReleaseTimeComparator>
      not_released_ops;
  for (const auto &op : machine_ops) {
    not_released_ops.push(op);
  }

  std::priority_queue<OperationDataToSingloMachine,
                      std::vector<OperationDataToSingloMachine>,
                      ProcessingTimeComparator>
      ready_ops;

  int current_time = 0;
  int max_completion_time = 0; // O Makespan (C_max)

  while (!not_released_ops.empty() || !ready_ops.empty()) {
    // --- Passo A: Liberar Operações ---
    while (!not_released_ops.empty() &&
           not_released_ops.top().release_time <= current_time) {
      ready_ops.push(not_released_ops.top());
      not_released_ops.pop();
    }

    // --- Passo B: Seleção da Operação (SPT) ---
    if (!ready_ops.empty()) {
      OperationDataToSingloMachine selected_op = ready_ops.top();
      ready_ops.pop();

      int start_time = std::max(current_time, selected_op.release_time);
      int completion_time = start_time + selected_op.duration;
      current_time = completion_time;
      max_completion_time = std::max(max_completion_time, completion_time);
    } else if (!not_released_ops.empty()) {
      // Se não há operações prontas, avança o tempo
      current_time = not_released_ops.top().release_time;
    }
  }
  return max_completion_time;
}

/**
 * Calculates the lower bound for a given search node.
 */
int calculate_lower_bound(const Node &node, const JobShopInstance &instance) {
  int max_lb = node.current_makespan;

  std::map<int, std::vector<OperationDataToSingloMachine>> ops_by_machine;

  // --- Parte 1: Limite Inferior Baseado em Job (Job-Based LB) ---
  for (int j = 0; j < instance.n_jobs; ++j) {
    int remaining_time_path_to_V = 0;
    for (int k = node.next_op_index[j]; k < instance.jobs[j].size(); ++k) {
      const Operation &op = instance.jobs[j][k];
      remaining_time_path_to_V += op.duration;
    }
    max_lb = std::max(max_lb,
                      node.job_completion_time[j] + remaining_time_path_to_V);

    // Prepara os dados para o Limite Inferior Baseado em Máquina
    if (node.next_op_index[j] < instance.jobs[j].size()) {
      const Operation &op = instance.jobs[j][node.next_op_index[j]];
      // r_ij é o Caminho Mais Longo U -> O_ij
      int r_ij = calculate_longest_path_U_to_O(node, op);
      int p_ij = op.duration;
      ops_by_machine[op.machine_id].push_back({op.job_id, p_ij, r_ij, 0});
    }
  }

  // --- Parte 2: Limite Inferior Baseado em Máquina (Machine-Based LB) ---
  for (int m = 0; m < instance.n_machines; ++m) {
    if (ops_by_machine.count(m)) {
      const auto &remaining_ops = ops_by_machine.at(m);
      // Resolve o problema ótimo 1 | r_j | C_max (SPT-Gulosa)
      int lb_machine = solve_single_machine_makespan_for_Lmax(remaining_ops);
      max_lb = std::max(max_lb, lb_machine);
    }
  }
  return max_lb;
}

} // end anonymous namespace

// --- BBSolver Implementation ---

BBSolver::BBSolver(const JobShopInstance &instance)
    : ISolver(instance),
      // Initialize with a heuristic solution (SPT, as used in Teste...)
      best_schedule(Schedule(instance)) {
  Schedule spt = DispatchSolver(instance, Rules::shortest_processing_time)
                     .solve(std::chrono::steady_clock::time_point::max());
  Schedule mwr = DispatchSolver(instance, Rules::most_work_remaining)
                     .solve(std::chrono::steady_clock::time_point::max());
  Schedule fcfs = DispatchSolver(instance, Rules::first_come_first_served)
                      .solve(std::chrono::steady_clock::time_point::max());
  Schedule lpt = DispatchSolver(instance, Rules::longest_processing_time)
                     .solve(std::chrono::steady_clock::time_point::max());
  Schedule random = DispatchSolver(instance, Rules::random_operation)
                        .solve(std::chrono::steady_clock::time_point::max());

  best_schedule = std::min({spt, mwr, fcfs, lpt /*, random*/},
                           [](const Schedule &a, const Schedule &b) -> bool {
                             return a.makespan() < b.makespan();
                           });
  // Store the makespan for faster comparisons
  upper_bound = best_schedule.makespan();
}

Schedule BBSolver::solve(
    std::chrono::steady_clock::time_point deadline) { // <-- Modified
  // Priority queue for Best-First Search (min-heap on lower_bound)
  std::priority_queue<std::shared_ptr<const Node>,
                      std::vector<std::shared_ptr<const Node>>, NodeComparator>
      queue;

  // 1. Initialize root node
  Node root;
  root.next_op_index.assign(instance.n_jobs, 0);
  root.machine_free_time.assign(instance.n_machines, 0);
  root.job_completion_time.assign(instance.n_jobs, 0);
  root.current_makespan = 0;
  root.lower_bound = calculate_lower_bound(root, instance);
  // root.partial_schedule is empty by default

  auto root_node_ptr = std::make_shared<Node>(root);

  queue.push(root_node_ptr);

  while (!queue.empty()) {
    // === TIMEOUT CHECK ===
    if (std::chrono::steady_clock::now() > deadline) {
      break; // Time limit reached, return the best solution found so far
    }

    auto current_node_ptr = queue.top();
    queue.pop();

    // === PODA (Pruning) ===
    if (current_node_ptr->lower_bound >= upper_bound) {
      continue;
    }

    // === VERIFICAÇÃO DE SOLUÇÃO (Goal Check) ===
    bool all_jobs_finished = true;
    for (int job = 0; job < instance.n_jobs; job++) {
      if (current_node_ptr->next_op_index[job] < instance.jobs[job].size()) {
        all_jobs_finished = false;
        break;
      }
    }

    if (all_jobs_finished) {
      // É uma solução completa. Verifica se é melhor que a atual.
      if (current_node_ptr->current_makespan < upper_bound) {
        upper_bound = current_node_ptr->current_makespan;

        best_schedule = Schedule(instance);
        std::vector<ScheduledOperation> ops;
        auto trace_ptr = current_node_ptr;

        while (trace_ptr->parent) { // Stop at root
          ops.push_back(trace_ptr->scheduled_op.value());
          trace_ptr = trace_ptr->parent;
        }
        // Add ops in correct order
        std::reverse(ops.begin(), ops.end());
        for (const auto &op : ops) {
          best_schedule.add(op);
        }
      }
      continue;
    }

    // === RAMIFICAÇÃO (Branching) ===

    // 1. Identificar operações elegíveis
    std::vector<Operation> eligible_ops;
    for (int job = 0; job < instance.n_jobs; ++job) {
      if (current_node_ptr->next_op_index[job] < instance.jobs[job].size()) {
        eligible_ops.push_back(
            instance.jobs[job][current_node_ptr->next_op_index[job]]);
      }
    }

    // 2. Estratégia de Branching: Focar na máquina "crítica" (i*)
    int min_completion_time = INT_MAX;
    int critical_machine_id = -1;

    for (const auto &op : eligible_ops) {
      // r_ij: Tempo que a operação PODE começar
      int r_ij = std::max(current_node_ptr->job_completion_time[op.job_id],
                          current_node_ptr->machine_free_time[op.machine_id]);
      // C_ij = r_ij + p_ij
      int c_ij = r_ij + op.duration;

      if (c_ij < min_completion_time) {
        min_completion_time = c_ij;
        critical_machine_id = op.machine_id;
      }
    }

    // 3. Ramificação Focada: Cria filhos APENAS para ops na máquina i*
    for (const auto &op_to_schedule : eligible_ops) {
      if (op_to_schedule.machine_id == critical_machine_id) {

        auto new_node_ptr = std::make_shared<Node>(*current_node_ptr);

        new_node_ptr->parent = current_node_ptr;

        // Determinar tempo de início (r_ij)
        int start_time =
            std::max(new_node_ptr->machine_free_time[op_to_schedule.machine_id],
                     new_node_ptr->job_completion_time[op_to_schedule.job_id]);

        int completion_time = start_time + op_to_schedule.duration;

        // Atualizar o estado do nó filho
        new_node_ptr->machine_free_time[op_to_schedule.machine_id] =
            completion_time;
        new_node_ptr->job_completion_time[op_to_schedule.job_id] =
            completion_time;
        new_node_ptr->current_makespan =
            std::max(new_node_ptr->current_makespan, completion_time);
        new_node_ptr->next_op_index[op_to_schedule.job_id]++;

        // **IMPORTANTE**: Adicionar ao schedule parcial com tempos
        new_node_ptr->scheduled_op =
            ScheduledOperation(op_to_schedule, start_time, completion_time);

        // Calcular novo LB e adicionar à fila
        new_node_ptr->lower_bound =
            calculate_lower_bound(*new_node_ptr, instance);
        if (new_node_ptr->lower_bound < upper_bound) {
          queue.push(new_node_ptr);
        }
      }
    }
  }

  // Retorna a melhor schedule completa encontrada
  return best_schedule;
}
