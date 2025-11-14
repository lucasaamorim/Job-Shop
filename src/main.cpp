#include <ISolver.h>
#include <JobShopInstance.h>
#include <Rules.h>
#include <Schedule.h>
#include <solvers/BBSolver.h>
#include <solvers/DispatchSolver.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

std::filesystem::path instance_path;
Rules::Rule rule_to_use;
std::string rule_name;
std::string algorithm_name;

const std::map<std::string, Rules::Rule> available_rules = {
    {"shortest_processing_time", Rules::shortest_processing_time},
    {"most_work_remaining", Rules::most_work_remaining},
    {"first_come_first_served", Rules::first_come_first_served},
    {"random_operation", Rules::random_operation},
    {"longest_processing_time", Rules::longest_processing_time}};

const std::set<std::string> available_algorithms = {"priority_dispatch",
                                                    "branch_and_bound"};

// Function declarations
JobShopInstance instance_from_file(const std::filesystem::path &filepath);
void print_usage(const char *prog_name);
void read_args(int argc, char *argv[]);

void print_usage(const char *prog_name) {
  std::cout << "Usage: " << prog_name << " [options] <PATH_TO_INSTANCE>"
            << std::endl;
  std::cout << "Arguments:" << std::endl;
  std::cout << "\t<PATH_TO_INSTANCE>" << std::endl;
  std::cout
      << "\t\tPath to an instance file or a directory. If a directory "
         "is provided, it will be searched recursively for instance files."
      << std::endl;
  std::cout << "Options:" << std::endl;
  // ALGORITHM
  std::cout
      << "\t-a, --algorithm <ALGORITHM_NAME>" << std::endl
      << "\t\tSpecify the algorithm to use. Defaults to 'priority_dispatch'."
      << std::endl
      << std::endl;
  std::cout << "\t\tAvailable algorithms:" << std::endl;
  for (const auto &name : available_algorithms) {
    std::cout << "\t\t  - " << name << std::endl;
  }
  // RULE
  std::cout << "\t-r, --rule <RULE_NAME>" << std::endl
            << "\t\tSpecify the dispatch rule to use (only for "
               "priority_dispatch). Defaults to "
               "'shortest_processing_time'."
            << std::endl
            << std::endl;
  std::cout << "\t\tAvailable rules:" << std::endl;
  for (const auto &[name, rule] : available_rules) {
    std::cout << "\t\t  - " << name << std::endl;
  }
  // HELP
  std::cout << "\t-h, --help" << std::endl
            << "\t\tDisplay this help message." << std::endl;
}

void read_args(int argc, char *argv[]) {
  // Defaults
  rule_name = "shortest_processing_time";
  rule_to_use = available_rules.at(rule_name);
  algorithm_name = "priority_dispatch";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      exit(0);
    } else if (arg == "-r" || arg == "--rule") {
      if (i + 1 < argc) {
        rule_name = argv[++i];
        if (available_rules.find(rule_name) == available_rules.end()) {
          std::cerr << "Error: Unknown rule '" << rule_name << "'."
                    << std::endl;
          print_usage(argv[0]);
          exit(1);
        }
        rule_to_use = available_rules.at(rule_name);
      } else {
        std::cerr << "Error: --rule option requires an argument." << std::endl;
        print_usage(argv[0]);
        exit(1);
      }
    } else if (arg == "-a" || arg == "--algorithm") {
      if (i + 1 < argc) {
        algorithm_name = argv[++i];
        if (available_algorithms.find(algorithm_name) ==
            available_algorithms.end()) {
          std::cerr << "Error: Unknown algorithm '" << algorithm_name << "'."
                    << std::endl;
          print_usage(argv[0]);
          exit(1);
        }
      } else {
        std::cerr << "Error: --algorithm option requires an argument."
                  << std::endl;
        print_usage(argv[0]);
        exit(1);
      }
    } else {
      instance_path = arg;
    }
  }

  if (instance_path.empty()) {
    std::cerr << "Error: Missing path to instance file or directory."
              << std::endl;
    print_usage(argv[0]);
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  read_args(argc, argv);

  std::vector<std::filesystem::path> instance_files;

  if (!std::filesystem::exists(instance_path)) {
    std::cerr << "Error: Path does not exist: " << instance_path << std::endl;
    return 1;
  }

  if (std::filesystem::is_directory(instance_path)) {
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(instance_path)) {
      if (entry.is_regular_file()) {
        instance_files.push_back(entry.path());
      }
    }
  } else {
    instance_files.push_back(instance_path);
  }

  std::string output_filename =
      algorithm_name +
      (algorithm_name == "priority_dispatch" ? "_" + rule_name : "") + ".csv";
  std::ofstream output_file(output_filename);

  if (!output_file.is_open()) {
    std::cerr << "Error: Could not open output file " << output_filename
              << std::endl;
    return 1;
  }

  // Write CSV header
  output_file << "Instance,Runtime (ms),Makespan\n";
  std::cout << "Algorithm: " << algorithm_name << std::endl;
  if (algorithm_name == "priority_dispatch") {
    std::cout << "Rule: " << rule_name << std::endl;
  }
  std::cout << "Output will be saved to " << output_filename << std::endl;
  std::cout << std::string(60, '-') << std::endl;
  std::cout << std::left << std::setw(30) << "Instance" << std::setw(15)
            << "Runtime (ms)" << std::setw(15) << "Makespan" << std::endl;
  std::cout << std::string(60, '-') << std::endl;

  for (const auto &file_path : instance_files) {
    try {
      std::string instance_name = file_path.stem().string();
      std::cout << std::left << std::setw(30) << instance_name << std::flush;

      JobShopInstance instance = instance_from_file(file_path);
      int makespan = 0;

      auto start_time = std::chrono::high_resolution_clock::now();

      std::unique_ptr<ISolver> solver;

      if (algorithm_name == "priority_dispatch") {
        // Cria um solver de despacho
        solver = std::make_unique<DispatchSolver>(instance, rule_to_use);
      } else if (algorithm_name == "branch_and_bound") {
        // Cria um solver B&B
        solver = std::make_unique<BBSolver>(instance);
      }

      // Se o solver foi instanciado, resolve
      if (solver) {
        Schedule schedule = solver->solve();
        makespan = schedule.makespan();
      } else {
        throw std::runtime_error("Algoritmo não reconhecido.");
      }

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                          end_time - start_time)
                          .count();

      std::cout << std::setw(15) << duration << std::setw(15) << makespan
                << std::endl;
      output_file << instance_name << "," << duration << "," << makespan
                  << "\n";

    } catch (const std::exception &e) {
      std::cerr << "Error processing file " << file_path.string() << ": "
                << e.what() << std::endl;
    }
  }
  std::cout << std::string(60, '-') << std::endl;
  std::cout << "Processing complete. Results saved to " << output_filename
            << std::endl;
  output_file.close();

  return 0;
}

JobShopInstance instance_from_file(const std::filesystem::path &filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file " + filepath.string());
  }

  int n_jobs, n_machines;
  file >> n_jobs >> n_machines;

  if (file.fail() || n_jobs <= 0 || n_machines <= 0) {
    throw std::runtime_error("Invalid header in file " + filepath.string());
  }

  std::vector<std::vector<Operation>> jobs(n_jobs,
                                           std::vector<Operation>(n_machines));
  for (int job_id = 0; job_id < n_jobs; ++job_id) {
    for (int op_idx = 0; op_idx < n_machines; ++op_idx) {
      int machine_id, duration;
      file >> machine_id >> duration;
      if (file.fail()) {
        throw std::runtime_error("Error reading operation data in " +
                                 filepath.string());
      }
      jobs[job_id][op_idx] = {
          job_id, machine_id, job_id * n_machines + op_idx, op_idx, duration,
      };
    }
  }

  return JobShopInstance(jobs, n_jobs, n_machines, n_jobs * n_machines);
}
