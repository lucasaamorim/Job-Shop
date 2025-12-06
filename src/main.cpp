#include <ISolver.h>
#include <JobShopInstance.h>
#include <Rules.h>
#include <Schedule.h>
#include <solvers/BBSolver.h>
#include <solvers/DispatchSolver.h>
#include <solvers/SASolver.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// --- Configuration ---

struct AppConfig {
  std::filesystem::path instance_path;
  std::string algorithm_name = "priority_dispatch";
  std::string rule_name = "shortest_processing_time";
  double timeout_seconds = std::numeric_limits<double>::infinity();

  static const std::map<std::string, Rules::Rule> AVAILABLE_RULES;
  static const std::set<std::string> AVAILABLE_ALGORITHMS;
};

const std::map<std::string, Rules::Rule> AppConfig::AVAILABLE_RULES = {
    {"shortest_processing_time", Rules::shortest_processing_time},
    {"most_work_remaining", Rules::most_work_remaining},
    {"first_come_first_served", Rules::first_come_first_served},
    {"random_operation", Rules::random_operation},
    {"longest_processing_time", Rules::longest_processing_time}};

const std::set<std::string> AppConfig::AVAILABLE_ALGORITHMS = {
    "priority_dispatch", "branch_and_bound", "simulated_annealing"};

// --- Function Declarations ---

void print_usage(const char *prog_name);
AppConfig parse_args(int argc, char *argv[]);
JobShopInstance instance_from_file(const std::filesystem::path &filepath);
std::vector<std::filesystem::path>
discover_files(const std::filesystem::path &path);
std::unique_ptr<ISolver> create_solver(const AppConfig &config,
                                       const JobShopInstance &instance);
std::ofstream create_output_file(const AppConfig &config);
void print_stdout_header(const AppConfig &config);
std::chrono::steady_clock::time_point
get_deadline(const std::chrono::steady_clock::time_point &start,
             double timeout_sec);

// --- Main Execution ---

int main(int argc, char *argv[]) {
  try {
    AppConfig config = parse_args(argc, argv);

    std::vector<std::filesystem::path> instance_files =
        discover_files(config.instance_path);
    std::ofstream output_file = create_output_file(config);
    output_file << "Instance,Runtime (ms),Makespan,TimedOut\n";

    print_stdout_header(config);

    for (const auto &file_path : instance_files) {
      try {
        std::string instance_name = file_path.stem().string();
        std::cout << std::left << std::setw(30) << instance_name << std::flush;

        JobShopInstance instance = instance_from_file(file_path);
        auto solver = create_solver(config, instance);

        auto start_time = std::chrono::steady_clock::now();
        auto deadline = get_deadline(start_time, config.timeout_seconds);

        Schedule schedule = solver->solve(deadline);
        int makespan = schedule.makespan();

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_time - start_time)
                            .count();

        bool timed_out = (end_time >= deadline);
        std::string timed_out_str = timed_out ? "Yes" : "No";

        std::cout << std::setw(15) << duration << std::setw(15) << makespan
                  << std::setw(10) << timed_out_str << std::endl;
        output_file << instance_name << "," << duration << "," << makespan
                    << "," << timed_out_str << "\n";

      } catch (const std::exception &e) {
        std::cerr << "\nError processing file " << file_path.string() << ": "
                  << e.what() << std::endl;
      }
    }

    std::cout << std::string(70, '-') << std::endl;
    std::cout << "Processing complete. Results saved to "
              << config.algorithm_name
              << (config.algorithm_name == "priority_dispatch"
                      ? "_" + config.rule_name
                      : "")
              << ".csv" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}

// --- Helper Implementations ---

/**
 * @brief Creates the correct solver object based on the app configuration.
 */
std::unique_ptr<ISolver> create_solver(const AppConfig &config,
                                       const JobShopInstance &instance) {
  if (config.algorithm_name == "priority_dispatch") {
    Rules::Rule rule_to_use = AppConfig::AVAILABLE_RULES.at(config.rule_name);
    return std::make_unique<DispatchSolver>(instance, rule_to_use);
  }
  if (config.algorithm_name == "branch_and_bound") {
    return std::make_unique<BBSolver>(instance);
  }
  if (config.algorithm_name == "simulated_annealing") {
      return std::make_unique<SASolver>(instance);
  }
  // This should be unreachable due to parse_args checks
  throw std::runtime_error("Unknown algorithm: " + config.algorithm_name);
}

/**
 * @brief Parses command-line arguments and returns an AppConfig struct.
 * Throws std::runtime_error on invalid arguments.
 */
AppConfig parse_args(int argc, char *argv[]) {
  AppConfig config;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      exit(0);
    } else if (arg == "-r" || arg == "--rule") {
      if (++i >= argc)
        throw std::runtime_error("--rule option requires an argument.");
      config.rule_name = argv[i];
      if (AppConfig::AVAILABLE_RULES.find(config.rule_name) ==
          AppConfig::AVAILABLE_RULES.end()) {
        throw std::runtime_error("Unknown rule: " + config.rule_name);
      }
    } else if (arg == "-a" || arg == "--algorithm") {
      if (++i >= argc)
        throw std::runtime_error("--algorithm option requires an argument.");
      config.algorithm_name = argv[i];
      if (AppConfig::AVAILABLE_ALGORITHMS.find(config.algorithm_name) ==
          AppConfig::AVAILABLE_ALGORITHMS.end()) {
        throw std::runtime_error("Unknown algorithm: " + config.algorithm_name);
      }
    } else if (arg == "-t" || arg == "--timeout") {
      if (++i >= argc)
        throw std::runtime_error("--timeout option requires an argument.");
      try {
        config.timeout_seconds = std::stod(argv[i]);
        if (config.timeout_seconds <= 0) {
          throw std::runtime_error("Timeout must be a positive number.");
        }
      } catch (const std::exception &e) {
        throw std::runtime_error(std::string("Invalid timeout value: ") +
                                 e.what());
      }
    } else if (config.instance_path.empty()) {
      config.instance_path = arg;
    } else {
      throw std::runtime_error("Unknown or duplicate argument: " + arg);
    }
  }

  if (config.instance_path.empty()) {
    throw std::runtime_error("Missing path to instance file or directory.");
  }

  return config;
}

/**
 * @brief Finds all regular files at a path, searching recursively if it's a
 * directory.
 */
std::vector<std::filesystem::path>
discover_files(const std::filesystem::path &path) {
  std::vector<std::filesystem::path> files;
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("Path does not exist: " + path.string());
  }

  if (std::filesystem::is_directory(path)) {
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(path)) {
      if (entry.is_regular_file()) {
        files.push_back(entry.path());
      }
    }
  } else if (std::filesystem::is_regular_file(path)) {
    files.push_back(path);
  }
  return files;
}

/**
 * @brief Creates and opens the CSV output file based on the config.
 */
std::ofstream create_output_file(const AppConfig &config) {
  std::string output_filename =
      config.algorithm_name +
      (config.algorithm_name == "priority_dispatch" ? "_" + config.rule_name
                                                    : "") +
      ".csv";
  std::ofstream output_file(output_filename);
  if (!output_file.is_open()) {
    throw std::runtime_error("Could not open output file " + output_filename);
  }
  return output_file;
}

/**
 * @brief Prints the run configuration to standard output.
 */
void print_stdout_header(const AppConfig &config) {
  std::cout << "Algorithm: " << config.algorithm_name << std::endl;
  if (config.algorithm_name == "priority_dispatch") {
    std::cout << "Rule: " << config.rule_name << std::endl;
  }
  if (config.timeout_seconds != std::numeric_limits<double>::infinity()) {
    std::cout << "Timeout: " << config.timeout_seconds << "s" << std::endl;
  }
  std::cout << std::string(70, '-') << std::endl;
  std::cout << std::left << std::setw(30) << "Instance" << std::setw(15)
            << "Runtime (ms)" << std::setw(15) << "Makespan" << std::setw(10)
            << "TimedOut" << std::endl;
  std::cout << std::string(70, '-') << std::endl;
}

/**
 * @brief Calculates the absolute deadline time point.
 */
std::chrono::steady_clock::time_point
get_deadline(const std::chrono::steady_clock::time_point &start,
             double timeout_sec) {
  if (timeout_sec == std::numeric_limits<double>::infinity()) {
    return std::chrono::steady_clock::time_point::max();
  }
  auto duration =
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
          std::chrono::duration<double>(timeout_sec));
  return start + duration;
}

/**
 * @brief Prints the command-line usage instructions.
 */
void print_usage(const char *prog_name) {
  std::cout << "Usage: " << prog_name << " [options] <PATH_TO_INSTANCE>"
            << std::endl;
  std::cout << "Arguments:" << std::endl;
  std::cout << "\t<PATH_TO_INSTANCE>" << std::endl;
  std::cout << "\t\tPath to an instance file or a directory (searched "
               "recursively)."
            << std::endl;
  std::cout << "Options:" << std::endl;
  // ALGORITHM
  std::cout << "\t-a, --algorithm <ALGORITHM_NAME> (Default: "
               "priority_dispatch)"
            << std::endl;
  std::cout << "\t\tAvailable algorithms:" << std::endl;
  for (const auto &name : AppConfig::AVAILABLE_ALGORITHMS) {
    std::cout << "\t\t  - " << name << std::endl;
  }
  // RULE
  std::cout << "\t-r, --rule <RULE_NAME> (Default: shortest_processing_time)"
            << std::endl;
  std::cout << "\t\t(Only for priority_dispatch)" << std::endl;
  std::cout << "\t\tAvailable rules:" << std::endl;
  for (const auto &[name, rule] : AppConfig::AVAILABLE_RULES) {
    std::cout << "\t\t  - " << name << std::endl;
  }
  // TIMEOUT
  std::cout << "\t-t, --timeout <SECONDS> (Default: no limit)" << std::endl;
  // HELP
  std::cout << "\t-h, --help" << std::endl
            << "\t\tDisplay this help message." << std::endl;
}

/**
 * @brief Parses an instance file and returns a JobShopInstance object.
 */
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
