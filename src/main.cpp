#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <getopt.h>
#include <fstream>
#include <string>
#include <cstring>
#include <dirent.h>

#include "led_controller.h"
#include "bandwidth_monitor.h"
#include "config_parser.h"
#include "led_state_manager.h"

// Global flag for graceful shutdown
volatile sig_atomic_t g_running = 1;

void signal_handler(int signal) {
    syslog(LOG_INFO, "Received signal %d, shutting down gracefully", signal);
    g_running = 0;
}

void setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
}

bool is_already_running(bool debug = false) {
    // Get current process ID
    pid_t current_pid = getpid();
    
    // Open /proc directory
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        return false; // If we can't check, assume it's safe to run
    }
    
    struct dirent* entry;
    std::string target_name = "ugreen_leds_ethutild";
    
    while ((entry = readdir(proc_dir)) != nullptr) {
        // Skip non-numeric directory names (only process IDs are numeric)
        if (!isdigit(entry->d_name[0])) {
            continue;
        }
        
        pid_t pid = atoi(entry->d_name);
        if (pid == current_pid || pid <= 0) {
            continue; // Skip our own process and invalid PIDs
        }
        
        // First check the comm file (process name)
        std::string comm_path = "/proc/" + std::string(entry->d_name) + "/comm";
        std::ifstream comm_file(comm_path);
        
        if (comm_file.is_open()) {
            std::string comm;
            std::getline(comm_file, comm);
            comm_file.close();
            
            // Remove any trailing newline
            if (!comm.empty() && comm.back() == '\n') {
                comm.pop_back();
            }
            
            // Check if this process has our exact name
            if (comm == target_name) {
                if (debug) {
                    std::cerr << "Found running instance via comm: PID " << pid << " (" << comm << ")" << std::endl;
                }
                closedir(proc_dir);
                return true; // Found another instance
            }
        }
        
        // Also check cmdline as fallback (for cases where comm is truncated)
        std::string cmdline_path = "/proc/" + std::string(entry->d_name) + "/cmdline";
        std::ifstream cmdline_file(cmdline_path);
        
        if (cmdline_file.is_open()) {
            std::string cmdline;
            std::getline(cmdline_file, cmdline);
            cmdline_file.close();
            
            // cmdline uses null bytes as separators, get first argument (executable path)
            size_t first_null = cmdline.find('\0');
            std::string executable = (first_null != std::string::npos) ?
                                   cmdline.substr(0, first_null) : cmdline;
            
            // Extract just the filename from the path
            size_t last_slash = executable.find_last_of('/');
            std::string exe_name = (last_slash != std::string::npos) ?
                                 executable.substr(last_slash + 1) : executable;
            
            // Check if the executable name matches exactly
            if (exe_name == target_name) {
                if (debug) {
                    std::cerr << "Found running instance via cmdline: PID " << pid << " (" << exe_name << ")" << std::endl;
                }
                closedir(proc_dir);
                return true; // Found another instance
            }
        }
    }
    
    closedir(proc_dir);
    return false; // No other instances found
}

void setup_logging(const std::string& log_level, bool console_mode = false) {
    int priority = LOG_INFO;
    
    if (log_level == "debug") {
        priority = LOG_DEBUG;
    } else if (log_level == "info") {
        priority = LOG_INFO;
    } else if (log_level == "warning") {
        priority = LOG_WARNING;
    } else if (log_level == "error") {
        priority = LOG_ERR;
    }
    
    int options = LOG_PID;
    if (console_mode) {
        options |= LOG_PERROR; // Also log to stderr
    }
    options |= LOG_CONS; // Fallback to console if syslog unavailable
    
    openlog("ugreen_leds_ethutild", options, LOG_DAEMON);
    setlogmask(LOG_UPTO(priority));
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -t, --test     Run in testing mode (cycles through bandwidth states)\n";
    std::cout << "  -h, --help     Show this help message\n";
    std::cout << "  -v, --version  Show version information\n";
    std::cout << "\nConfiguration:\n";
    std::cout << "  The service looks for configuration in:\n";
    std::cout << "    1. ./ugreen_leds_ethutild.conf\n";
    std::cout << "    2. /etc/ugreen_leds_ethutild.conf\n";
    std::cout << "  If no config file is found, defaults are used.\n";
}

void print_version() {
    std::cout << "ugreen_leds_ethutild version 1.0.0\n";
    std::cout << "UGREEN LEDs Ethernet Utilization Daemon for NAS bandwidth monitoring\n";
}

bool run_testing_mode(led_state_manager_t& state_manager) {
    syslog(LOG_INFO, "Starting testing mode - cycling through bandwidth states");
    std::cout << "Testing mode: cycling through bandwidth states (Ctrl+C to stop)\n";
    
    // Test states with corresponding usage percentages
    struct test_state_t {
        double usage_percentage;
        const char* description;
    };
    
    test_state_t test_states[] = {
        {5.0, "5% usage - Power white, utilization LEDs off"},
        {25.0, "25% usage - Power white, NetDev green"},
        {60.0, "60% usage - Power white, NetDev + Disk1 blue"},
        {90.0, "90% usage - Power white, NetDev + Disk1 + Disk2 red"}
    };
    
    int state_count = sizeof(test_states) / sizeof(test_states[0]);
    int current_state = 0;
    
    while (g_running) {
        const auto& test_state = test_states[current_state];
        
        std::cout << test_state.description << std::endl;
        syslog(LOG_INFO, "Testing: %s", test_state.description);
        
        // Create fake bandwidth info
        bandwidth_info_t fake_bandwidth = {
            .rx_mbps = test_state.usage_percentage * 10.0,  // Fake values
            .tx_mbps = test_state.usage_percentage * 10.0,
            .total_mbps = test_state.usage_percentage * 20.0,
            .usage_percentage = test_state.usage_percentage,
            .valid = true
        };
        
        if (!state_manager.update_leds(fake_bandwidth)) {
            syslog(LOG_ERR, "Failed to update LEDs in testing mode");
            return false;
        }
        
        // Wait 1 second before next state
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        current_state = (current_state + 1) % state_count;
    }
    
    syslog(LOG_INFO, "Testing mode completed");
    return true;
}

bool run_normal_mode(bandwidth_monitor_t& bandwidth_monitor, led_state_manager_t& state_manager) {
    syslog(LOG_INFO, "Starting normal monitoring mode");
    
    if (!bandwidth_monitor.initialize()) {
        syslog(LOG_ERR, "Failed to initialize bandwidth monitor for interface %s",
               bandwidth_monitor.get_interface().c_str());
        std::cerr << "Error: Failed to initialize bandwidth monitor for interface "
                  << bandwidth_monitor.get_interface() << std::endl;
        std::cerr << "Please check that the network interface exists and is active." << std::endl;
        return false;
    }
    
    syslog(LOG_INFO, "Monitoring interface: %s (capacity: %u Mbps)",
           bandwidth_monitor.get_interface().c_str(),
           bandwidth_monitor.get_capacity_mbps());
    
    // Wait 1 second after initialization to ensure first measurement is valid
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    int consecutive_failures = 0;
    const int max_failures = 10;
    
    while (g_running) {
        auto bandwidth_info = bandwidth_monitor.get_bandwidth_usage();
        
        if (bandwidth_info.valid) {
            consecutive_failures = 0; // Reset failure counter
            
            syslog(LOG_DEBUG, "Bandwidth: RX=%.1f Mbps, TX=%.1f Mbps, Total=%.1f Mbps (%.1f%%)",
                   bandwidth_info.rx_mbps, bandwidth_info.tx_mbps,
                   bandwidth_info.total_mbps, bandwidth_info.usage_percentage);
            
            if (!state_manager.update_leds(bandwidth_info)) {
                syslog(LOG_WARNING, "Failed to update LEDs");
            }
        } else {
            consecutive_failures++;
            syslog(LOG_WARNING, "Invalid bandwidth measurement (failure %d/%d)",
                   consecutive_failures, max_failures);
            
            if (consecutive_failures >= max_failures) {
                syslog(LOG_ERR, "Too many consecutive bandwidth measurement failures, exiting");
                return false;
            }
        }
        
        // Wait 1 second before next measurement
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    syslog(LOG_INFO, "Normal monitoring mode completed");
    return true;
}

int main(int argc, char* argv[]) {
    // Check if another instance is already running
    if (is_already_running()) {
        std::cerr << "Error: Another instance of ugreen_leds_ethutild is already running." << std::endl;
        std::cerr << "Only one instance is allowed to prevent conflicts." << std::endl;
        std::cerr << "Please stop the existing instance before starting a new one." << std::endl;
        return 1;
    }
    
    bool test_mode = false;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"test", no_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "thv", long_options, nullptr)) != -1) {
        switch (c) {
            case 't':
                test_mode = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                print_version();
                return 0;
            case '?':
                print_usage(argv[0]);
                return 1;
            default:
                break;
        }
    }
    
    // Load configuration
    config_parser_t config_parser;
    ledctl_config_t config;
    
    if (!config_parser.load_config(config)) {
        std::cerr << "Failed to load configuration" << std::endl;
        return 1;
    }
    
    // Setup logging (enable console output for interactive use)
    bool console_mode = isatty(STDERR_FILENO) || test_mode;
    setup_logging(config.log_level, console_mode);
    setup_signal_handlers();
    
    syslog(LOG_INFO, "LED Control Service starting (interface: %s, capacity: %u Mbps, brightness: %u, thresholds: %u/%u/%u%%)",
           config.interface.c_str(), config.capacity_mbps, config.brightness,
           config.low_threshold, config.medium_threshold, config.high_threshold);
    
    // Initialize LED controller
    led_controller_t led_controller;
    if (led_controller.start() != 0) {
        syslog(LOG_ERR, "Failed to initialize LED controller");
        std::cerr << "Error: Failed to initialize LED controller" << std::endl;
        std::cerr << "Please check that:" << std::endl;
        std::cerr << "  1. You have root permissions" << std::endl;
        std::cerr << "  2. The i2c-dev module is loaded" << std::endl;
        std::cerr << "  3. The hardware is compatible" << std::endl;
        return 1;
    }
    
    // Initialize LED state manager
    led_state_manager_t state_manager(led_controller, config);
    
    // Set initial state (power LED on, utilization LEDs off)
    state_manager.set_state(led_state_t::UTILIZATION_OFF);
    
    bool success = false;
    
    if (test_mode) {
        success = run_testing_mode(state_manager);
    } else {
        // Initialize bandwidth monitor
        bandwidth_monitor_t bandwidth_monitor(config.interface, config.capacity_mbps);
        success = run_normal_mode(bandwidth_monitor, state_manager);
    }
    
    // Turn off all LEDs before exit (including power LED)
    syslog(LOG_INFO, "Turning off all LEDs before shutdown");
    led_controller.turn_off_all_leds();
    
    syslog(LOG_INFO, "LED Control Service stopped");
    closelog();
    
    return success ? 0 : 1;
}