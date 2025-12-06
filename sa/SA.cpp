#include "JobShopInstance.h"
#include "Schedule.h"
#include "ScheduleState.h"
#include "solvers/DispatchSolver.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>
#include <list>
#include <ctime>

#define WARMING 1
#define COOLING 0
#define MAX_ATTEMPTS 1000000

// A sequência manipulada pelo SA será uma lista de todas as operações,
// que representa a ordem de prioridade para o despacho.
using Sequence = std::vector<Operation>;

// Implementação do Critério de Metropolis
bool successChance(int delta_cmax, double temperature) {
    if (delta_cmax < 0) {
        return true; // Aceita melhorias
    }
    if (temperature <= 0.0) {
        return false; // Não aceita pioras se a temperatura for zero ou negativa
    }
    // Fórmula clássica: P = e^(-Delta_E / T)
    double acceptance_probability = std::exp(-((double)delta_cmax / temperature));
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    return dis(gen) < acceptance_probability;
}

// --- CLASSE JOB SHOP SOLVER ---
// Esta classe contém a instância e os métodos de avaliação/perturbação
class JobShopSolver {
private:
    JobShopInstance instance;
    std::vector<Operation> all_operations_linear; // Lista linear de todas as operações (para o SA)

    // Gera a sequência inicial.
    Sequence generateInitialSequence() {
      // Run a few dispatch rules to get a good initial upper bound.
      // A tighter initial bound leads to more pruning.
      std::vector<Schedule> heuristic_schedules;
      heuristic_schedules.push_back(
          DispatchSolver(instance, Rules::shortest_processing_time)
              .solve(std::chrono::steady_clock::time_point::max()));
      heuristic_schedules.push_back(
          DispatchSolver(instance, Rules::most_work_remaining)
              .solve(std::chrono::steady_clock::time_point::max()));
      heuristic_schedules.push_back(
          DispatchSolver(instance, Rules::first_come_first_served)
              .solve(std::chrono::steady_clock::time_point::max()));
      heuristic_schedules.push_back(
          DispatchSolver(instance, Rules::longest_processing_time)
              .solve(std::chrono::steady_clock::time_point::max()));

      // Find the best schedule among the heuristics
      auto best_it =
          std::min_element(heuristic_schedules.begin(), heuristic_schedules.end(),
                           [](const Schedule &a, const Schedule &b) {
                             return a.makespan() < b.makespan();
                           });
      auto best_scheduled = *best_it;
      for (const auto& schedule : best_scheduled.schedule) {
        for (const auto& op : schedule) {
            all_operations_linear.push_back(op);
        }
      }

      return ;
    }


    // Avalia uma sequência (lista de prioridade) rodando o Dispatcher
    // Retorna o makespan (o custo) e, por referência, o Schedule
    int evaluateSequence(const Sequence& seq, Schedule& result_schedule) {
        ScheduleState s(instance);
        
        // Mapear operações para saber quais já foram despachadas.
        // O ScheduleState usa o operation_id, que deve ser único.
        std::vector<bool> dispatched(all_operations_linear.size(), false);
        
        // Loop principal de Despacho
        while (!s.isDone()) {
            bool found_available = false;
            
            // Itera sobre a Sequence (ordem de prioridade)
            for (int i = 0; i < seq.size(); ++i) {
                if (!dispatched[seq[i].operation_id - 1]) { // Assumindo operation_id >= 1
                    const Operation& op_to_check = seq[i];
                    
                    // Verifica se a operação de maior prioridade na sequência está 'disponível'
                    bool is_available = false;
                    for (const auto& available_op : s.available_operations) {
                        if (available_op.operation_id == op_to_check.operation_id) {
                            is_available = true;
                            break;
                        }
                    }

                    if (is_available) {
                        s.dispatch(op_to_check); // Despacha e atualiza o estado
                        dispatched[op_to_check.operation_id - 1] = true;
                        found_available = true;
                        break; // Sai do loop 'for' para recalcular as operações disponíveis
                    }
                }
            }
            if (!found_available && !s.isDone()) {
                // Caso de erro na simulação, embora não deva ocorrer com a lógica ScheduleState.
                break;
            }
        }
        
        result_schedule = s.schedule;
        return result_schedule.makespan();
    }

    // Gerador de Vizinhos: Troca a posição de duas operações na mesma máquina
    Sequence generateNeighborSequence(const Sequence& current_seq) {
        Sequence neighbor = current_seq;
        
        if (neighbor.size() < 2) return neighbor;

        // Tenta encontrar dois índices de operações na mesma máquina para trocar
        int attempts = 0;
        int idx1 = -1, idx2 = -1;
        std::random_device rd;
        std::mt19937 g(rd());
        std::uniform_int_distribution<> dis(0, neighbor.size() - 1);

        while (attempts < MAX_ATTEMPTS) {
            idx1 = dis(g);
            idx2 = dis(g);

            if (idx1 != idx2 && neighbor[idx1].machine_id == neighbor[idx2].machine_id) {
                break; // Encontrado
            }
            attempts++;
        }

        if (attempts < MAX_ATTEMPTS) {
            std::swap(neighbor[idx1], neighbor[idx2]);
        }
        return neighbor;
    }
    
public:
    JobShopSolver(const JobShopInstance& inst) : instance(inst) {
        // Popula a lista linear de todas as operações (assume operation_id é o índice + 1)
        for (const auto& job : instance.jobs) {
            for (const auto& op : job) {
                all_operations_linear.push_back(op);
            }
        }
    }
    
    // Função principal do Simulated Annealing (adaptada)
    Schedule solveUsingSA(double initial_temperature, double alpha_warming, double alpha_cooling, int cooling_length, double warming_threshold, int max_moves_without_improvement)
    {
        // ... (Corpo do SA) ...
        // 1. INICIALIZAÇÃO
        std::srand(std::time(NULL));
        int mode = WARMING;
        double temperature = initial_temperature;

        // Variáveis de rastreamento de Aquecimento
        std::list<bool> last_moves;
        const int last_moves_size = 2000;
        last_moves.resize(last_moves_size);
        std::fill(last_moves.begin(), last_moves.end(), false);

        int accepted_moves = 0;
        int accepted_moves_out_of_last_moves = 0;
        int moves_without_improvement = 0;

        // Soluções e Custos
        Schedule S_temp(instance); // Schedule temporário para avaliação
        Sequence S_current = generateInitialSequence();
        int cmax = evaluateSequence(S_current, S_temp);
        
        Sequence S_best = S_current;
        int best_cmax = cmax;
        
        // Inicialização de tempo
        struct timespec start, stop;
        double totaltime = 0.0;
        clock_gettime(CLOCK_REALTIME, &start);

        // 2. LOOP PRINCIPAL
        while (moves_without_improvement < max_moves_without_improvement && totaltime < 300.0) { // 300s = 5 minutos
            // 2a. Geração de Vizinho S'
            Sequence S_neighbor = generateNeighborSequence(S_current);
            Schedule S_neighbor_schedule(instance);
            int new_cmax = evaluateSequence(S_neighbor, S_neighbor_schedule);
            int delta_cmax = new_cmax - cmax;

            bool move_accepted = false;

            // 2b. Critério de Aceitação
            if (delta_cmax <= 0) { // Melhor ou Igual (Aceitação Certa)
                move_accepted = true;
            } else { // Pior (Aceitação Probabilística)
                if (successChance(delta_cmax, temperature)) {
                    move_accepted = true;
                }
            }

            // 2c. Atualização de Estados e Contadores
            if (move_accepted) {
                S_current = S_neighbor;
                cmax = new_cmax;
                accepted_moves++;
                // Rastreamento de Aquecimento
                if (mode == WARMING) {
                    last_moves.push_back(true);
                    if (last_moves.front() == false) accepted_moves_out_of_last_moves++;
                    last_moves.pop_front();
                }
            } else {
                // Movimento rejeitado
                if (mode == WARMING) {
                    // Ajuste de Temperatura (Aquecimento)
                    if (accepted_moves > cooling_length / 10) {
                        accepted_moves = 0;
                        temperature += alpha_warming * initial_temperature;
                    }
                    
                    last_moves.push_back(false);
                    if (last_moves.front() == true) accepted_moves_out_of_last_moves--;
                    last_moves.pop_front();
                }
            }
            
            // 2d. Rastreamento da Melhor Solução Global
            if (cmax < best_cmax) {
                best_cmax = cmax;
                moves_without_improvement = 0; 
                S_best = S_current; // Salvar a nova melhor sequência
            } else {
                moves_without_improvement++; 
            }

            // 2e. ATUALIZAÇÃO DA TEMPERATURA (WARMING/COOLING SCHEDULE)
            if (mode == WARMING && (double)accepted_moves_out_of_last_moves / last_moves_size >= warming_threshold) {
                accepted_moves = 0;
                mode = COOLING; // Transição para Resfriamento
            } else if (mode == COOLING && accepted_moves >= cooling_length) {
                temperature *= alpha_cooling; // Redução da Temperatura
                accepted_moves = 0;
            }

            // 2f. CRITÉRIO DE PARADA POR TEMPO
            clock_gettime(CLOCK_REALTIME, &stop);
            totaltime += (double)(stop.tv_sec - start.tv_sec) + 1.e-9 * (stop.tv_nsec - start.tv_nsec);
            clock_gettime(CLOCK_REALTIME, &start);
        }
        
        // 3. FINALIZAÇÃO: Avalia a melhor sequência para obter o Schedule completo.
        Schedule final_best_schedule(instance);
        evaluateSequence(S_best, final_best_schedule); 

        std::cout << "SA concluído. Melhor Makespan: " << best_cmax << ", Tempo em sec: " << totaltime << std::endl;
        
        return final_best_schedule;
    }
};