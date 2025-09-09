#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#endif

void print_usage(const char* prog_name) {
    std::cerr << "A cross-platform tool to set CPU affinity for a process." << std::endl;
    std::cerr << "Usage: " << prog_name << " <pid> <core_id>" << std::endl;
    std::cerr << "Example (Linux):   sudo " << prog_name << " 12345 0" << std::endl;
    std::cerr << "Example (Windows): " << prog_name << " 6789 1" << std::endl;
    std::cerr << "\nNote: This tool usually requires administrator/root privileges." << std::endl;
}

unsigned int get_hardware_concurrency() {
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwNumberOfProcessors;
#else
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return (nprocs > 0) ? static_cast<unsigned int>(nprocs) : 0;
#endif
}

#ifdef _WIN32
void set_process_affinity(unsigned long pid, int core_id) {
    std::cout << "Running on Windows." << std::endl;

    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        throw std::runtime_error("Could not open process with PID " + std::to_string(pid) +
                                 ". Error code: " + std::to_string(GetLastError()));
    }

    DWORD_PTR affinity_mask = 1ULL << core_id;

    if (!SetProcessAffinityMask(hProcess, affinity_mask)) {
        CloseHandle(hProcess);
        throw std::runtime_error("Failed to set process affinity mask. Error code: " + std::to_string(GetLastError()));
    }

    std::cout << "Successfully set affinity for PID " << pid << " to core " << core_id << std::endl;

    CloseHandle(hProcess);
}

#elif __linux__
void set_process_affinity(pid_t pid, int core_id) {
    std::cout << "Running on Linux." << std::endl;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int result = sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        throw std::runtime_error("sched_setaffinity failed: " + std::string(strerror(errno)));
    }

    std::cout << "Successfully set affinity for PID " << pid << " to core " << core_id << std::endl;
}

#else
void set_process_affinity(long pid, int core_id) {
    throw std::runtime_error("Unsupported operating system.");
}
#endif

int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    long pid_long;
    int core_id;

    try {
        pid_long = std::stol(argv[1]);
        core_id = std::stoi(argv[2]);
    } catch (const std::invalid_argument&) {
        std::cerr << "Error: Invalid argument. PID and core_id must be integers." << std::endl;
        print_usage(argv[0]);
        return 1;
    } catch (const std::out_of_range&) {
        std::cerr << "Error: Argument is out of range." << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    unsigned int num_cpus = get_hardware_concurrency();
    if (num_cpus > 0 && (core_id < 0 || static_cast<unsigned int>(core_id) >= num_cpus)) {
        std::cerr << "Error: Core ID " << core_id << " is out of range. "
                  << "Available cores on this system: 0 to " << num_cpus - 1 << "." << std::endl;
        return 1;
    }

    try {
        set_process_affinity(static_cast<decltype(pid_long)>(pid_long), core_id);
    } catch (const std::runtime_error& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        std::cerr << "Please ensure the PID is correct and you have sufficient privileges (e.g., run with 'sudo' or as Administrator)." << std::endl;
        return 1;
    }

    return 0;
}
