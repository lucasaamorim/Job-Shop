#include <Solver.h>
#include <JobShopInstance.h>
#include <Operation.h>
#include <Rules.h>
#include <Schedule.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

std::filesystem::path instance_path;
Rules::Rule rule_to_use;
std::string rule_name;
std::map<std::string, Rules::Rule> available_rules;

// Function declarations
void initialize_rules();
JobShopInstance instance_from_file(const std::filesystem::path &filepath);
void print_usage(const char *prog_name);
void read_args(int argc, char *argv[]);

// Populates the map of available dispatching rules
void initialize_rules() {
  available_rules["shortest_processing_time"] = Rules::shortest_processing_time;
  available_rules["most_work_remaining"] = Rules::most_work_remaining;
  available_rules["first_come_first_served"] = Rules::first_come_first_served;
  available_rules["random_operation"] = Rules::random_operation;
  available_rules["longest_processing_time"] = Rules::longest_processing_time;
}

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
  std::cout << "\t-h, --help" << std::endl
            << "\t\tDisplay this help message." << std::endl;
  std::cout << "\t-r, --rule <RULE_NAME>" << std::endl
            << "\t\tSpecify the dispatch rule to use. Defaults to "
               "'shortest_processing_time'."
            << std::endl
            << std::endl;
  std::cout << "\t\tAvailable rules:" << std::endl;
  for (const auto &[name, rule] : available_rules) {
    std::cout << "\t\t  - " << name << std::endl;
  }
}

void read_args(int argc, char *argv[]) {
  // Default rule
  rule_name = "shortest_processing_time";
  rule_to_use = available_rules[rule_name];

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
        rule_to_use = available_rules[rule_name];
      } else {
        std::cerr << "Error: --rule option requires an argument." << std::endl;
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
  initialize_rules();
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

  std::string output_filename = rule_name + ".csv";
  std::ofstream output_file(output_filename);

  if (!output_file.is_open()) {
    std::cerr << "Error: Could not open output file " << output_filename
              << std::endl;
    return 1;
  }

  // Write CSV header
  output_file << "Instance,Runtime (ms),Makespan\n";
  std::cout << "Using rule: " << rule_name << ". Output will be saved to "
            << output_filename << std::endl;
  std::cout << std::string(60, '-') << std::endl;
  std::cout << std::left << std::setw(30) << "Instance" << std::setw(15)
            << "Runtime (ms)" << std::setw(15) << "Makespan" << std::endl;
  std::cout << std::string(60, '-') << std::endl;

  for (const auto &file_path : instance_files) {
    try {
      std::string instance_name = file_path.stem().string();
      std::cout << std::left << std::setw(30) << instance_name << std::flush;
      auto start_time = std::chrono::high_resolution_clock::now();

      JobShopInstance instance = instance_from_file(file_path);
      Solver solver(instance, rule_to_use);
      Schedule schedule = solver.solve();
      int makespan = schedule.makespan();

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
      jobs[job_id][op_idx] = {job_id, op_idx, machine_id, duration,
                              job_id * n_machines + op_idx};
    }
  }

  return JobShopInstance(jobs, n_machines);
}
