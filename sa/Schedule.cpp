#include "Schedule.h"

#include <algorithm>
#include <numeric>

void Schedule::add(ScheduledOperation op) {
    schedule[op.machine_id].insert(op);
}

bool Schedule::is_complete() {
  int n_scheduled =
      std::accumulate(schedule.begin(), schedule.end(), 0,
                      [](int acc, const op_set &ops) {
                        return acc + ops.size();
                      });
  return n_scheduled == instance.n_operations;
}

int Schedule::makespan() const{
  // O 'makespan' geral começa em 0
  int max_makespan = 0;

  // Itera por cada máquina (cada op_set no vetor)
  for (const auto &machine_schedule : schedule) {
    // Verifica se o set da máquina NÃO está vazio
    if (!machine_schedule.empty()) {
      // Pega o end_time da última operação dessa máquina
      int machine_end_time = machine_schedule.rbegin()->end_time;

      // Atualiza o makespan geral se o desta máquina for maior
      if (machine_end_time > max_makespan) {
        max_makespan = machine_end_time;
      }
    }
    // Se machine_schedule estiver vazio, seu makespan é 0,
    // então ele é ignorado (corretamente) pelo 'if'.
  }

  return max_makespan;
}
