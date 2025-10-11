#include <bits/stdc++.h>
using namespace std;

constexpr int INF = 1e9;
constexpr int EXACT_LIMIT = 8; 

// Representa uma operação (O processamento de um trabalho em uma máquina)
struct Operation {
  int id; // Id da operação
  int job; // Id do trabalho
  int machine; // Id da máquina
  int time; // Tempo de processamento da operação
};

// Representa uma instância (um conjunto de dados de entrada) do problema
struct Instance {
  vector<Operation> operations; // Vetor contendo todas as operações
  map<int, vector<int>> jobsOperations; // Mapa que mapeia a cada job, o vetor das operações de cada job, na ordem em que devem ser executadas
  map<int, vector<int>> machineOperations; // Mapa que mapeia a cada máquina, uma lista de ids das operações que precisam ser feitas nessa máquina
  set<int> machines; // Conjunto de máquinas da instância
  
  /**
   * @brief Adiciona um novo job na instância
   * @param jobId Id do job
   * @param route Representa a sequência de máquinas que o job vai percorrer, na ordem em que cada operação deve ser executada
   * @param timeOps Representa o tempo de processamento de cada operação
   * @attention O tamanho de route deve ser igual ao de timeOPs, pois cada índice i do opTime representa o tempo para que a operação do job na máquina do route[i] seja executada
   */
  void addJob(int jobId, const vector<int> &route, const vector<int> &opTime) {
    for(int i=0;i<route.size();i++) {
      Operation op;
      op.id = operations.size();
      op.job = jobId;
      op.machine = route[i];
      op.time = opTime[i];
      operations.push_back(op);
      jobsOperations[jobId].push_back(op.id);
      machineOperations[op.machine].push_back(op.id);
      machines.insert(op.machine);
    }
  }
};

/**
 * @brief Constrói o grafo direcionado de precedências (na forma de lista de adjacência).
 *
 * O grafo inclui:
 *  - Arcos conjuntivos: representam a ordem fixa das operações dentro de cada job.
 *  - Arcos disjuntivos orientados: representam a ordem já decidida das operações em cada máquina (definida em machineOrders).
 *
 * @param inst Instância do problema (contém as operações e suas sequências por job).
 * @param machineOrders Um mapa (máquina → vetor de ids de operações) que contém, para algumas máquinas, a sequência já decidida das operações nessa máquina.
 * @return Lista de adjacência (vector<vector<int>>) representando o grafo resultante.
 */
vector<vector<int>> buildAdjacency(const Instance &inst,
                                  const unordered_map<int, vector<int>> &machineOrders) {
    int numOperations = inst.operations.size();
    vector<vector<int>> adj(numOperations);
    // Adiciona os arcos conjuntivos (coloca as arestas dirigidas u->v à lista de adjacência)
    for (const auto &jobOperations : inst.jobsOperations) {
        for (size_t i = 1; i < jobOperations.second.size(); ++i) {
            int u = jobOperations.second[i-1];
            int v = jobOperations.second[i];
            adj[u].push_back(v);
        }
    }
    // Inclui os arcos disjuntivos orientados
    for (const auto &kv : machineOrders) {
        const vector<int> &ord = kv.second;
        for (size_t i = 0; i < ord.size(); ++i) {
            for (size_t j = i+1; j < ord.size(); ++j) {
                adj[ord[i]].push_back(ord[j]);
            }
        }
    }
    return adj;
}

/**
 * @brief Ordenação topológica de Kahn
 * @return Um par contendo um bool que indica se o grafo é acíclico (true) ou não (false) e um vetor que possui a ordem topológica das operações.
 */
pair<bool,vector<int>> topologicalSort(const vector<vector<int>> &adj) {
  int numVertices = adj.size();
  vector<int> topologicalOrder;
  // Vetor contendo graus de entrada de cada vértice
  vector<int> entryDegrees(numVertices);
  // Calculando grau de entrada de cada vértice.
  for(vector<int> vertex: adj) {
    for(int adjVertex: vertex) {
      entryDegrees[adjVertex]++;
    }
  }
  queue<int> vertexWithDegree0;
  for(int vertex=0;vertex<entryDegrees.size();vertex++) {
    if(entryDegrees[vertex] == 0) vertexWithDegree0.push(vertex);
  }
  while(!vertexWithDegree0.empty()){
    int vertex = vertexWithDegree0.front();
    vertexWithDegree0.pop();
    topologicalOrder.push_back(vertex);
    for(int incVertex: adj[vertex]) {
      entryDegrees[incVertex]--;
      if(entryDegrees[incVertex] == 0) vertexWithDegree0.push(incVertex);
    }
  }
  if(topologicalOrder.size() != numVertices) return {false, {}};
  return {true, topologicalOrder};    
}

/**
 * @brief Computa o tempo de início mais cedo e o tempo de conclusão mais cedo de cada operação.
 * @return Retorna uma tupla que inclui:
 *  - Um booleano que indica se o grafo é acíclico (true) ou não (false);
 *  - Um vetor contendo o tempo de início mais cedo de cada operação.
 *  - Um vetor contendo o tempo de conclusão mais cedo de cada operação.
 */
tuple<bool, vector<int>, vector<int>> forwardPass(const Instance &inst, const vector<vector<int>> &adj) {
  int numOperations = inst.operations.size();
  auto pairOrderTopological = topologicalSort(adj);
  bool isDAG = pairOrderTopological.first;
  if(!isDAG) return {false, {}, {}};
  vector<int> orderTopological = pairOrderTopological.second;
  vector<int> earliestStarts(numOperations, 0), earliestCompletions(numOperations, 0);
  vector<vector<int>> predecessors(numOperations);
  // Encontrando os predecessores de cada vértice
  for(int pred: orderTopological) {
    for(int vertex: adj[pred]) {
      predecessors[vertex].push_back(pred);
    }
  }
  // Calculando tempo de início mais cedo e tempo de conclusão mais cedo de cada operação
  for(int vertex: orderTopological) {
    int start = 0;
    for(int p: predecessors[vertex]) {
      start = max(start, earliestCompletions[p]);
    }
    earliestStarts[vertex] = start;
    earliestCompletions[vertex] = start+inst.operations[vertex].time;
  }
  return {true, earliestStarts, earliestCompletions};
}

/**
 * @brief Computa o início mais tardio (latestStarts) e a conclusão mais tardia (latestCompletions) de cada operação.
 * 
 * A partir do grafo de precedências e dos tempos de conclusão obtidos no forward pass,
 * esta função realiza uma passagem no sentido inverso da ordem topológica (backward pass)
 * para determinar até quando cada operação pode ser atrasada sem aumentar o makespan do sistema.
 *
 * @param inst Instância contendo as operações e seus tempos de processamento.
 * @param adj Lista de adjacência representando o grafo de precedências entre operações.
 * @param earliestCompletion Vetor de tempos de conclusão mais cedo (obtido no forward pass).
 * @return Um par (latestStarts, latestCompletions) com os tempos de início e término mais tardios de cada operação.
 */

pair<vector<int>, vector<int>> backwardPass(const Instance &inst, const vector<vector<int>> &adj, const vector<int> &earliestCompletion) {
    int numOperations = inst.operations.size();
    auto pairOrderTopological = topologicalSort(adj);
    bool isDAG = pairOrderTopological.first;
    if(!isDAG) return {{}, {}};
    vector<int> orderTopological = pairOrderTopological.second;
    vector<int> latestStarts(numOperations, INF), latestCompletions(numOperations, INF);
    // Calculando makespan (earlistCompletions_max)
    int makespan = 0;
    for(int c: earliestCompletion) makespan = max(makespan, c);
    // Calculando tempo de início mais tarde e tempo de conclusão mais tarde de cada operação
    for(int i=orderTopological.size(); i>=0; i++) {
        int vertex = orderTopological[i];
        // Verificando se o vértice tem sucessores
        if(adj[vertex].empty()) {
            latestCompletions[vertex] = makespan;
        } else {
            int best = makespan;
            for(int s: adj[vertex]) {
                best = min(best, latestStarts[s]);
            }
            latestCompletions[vertex] = best; 
        }
        latestStarts[vertex] = latestCompletions[vertex] - inst.operations[vertex].time; 
    }
    return {latestStarts, latestCompletions};
}