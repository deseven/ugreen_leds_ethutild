#include "bandwidth_monitor.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <syslog.h>

bandwidth_monitor_t::bandwidth_monitor_t(const std::string& interface, uint32_t capacity_mbps)
    : _interface(interface), _capacity_mbps(capacity_mbps), _initialized(false) {
}

bool bandwidth_monitor_t::initialize() {
    // Check if interface exists first
    std::string sys_path = "/sys/class/net/" + _interface;
    if (!std::filesystem::exists(sys_path)) {
        syslog(LOG_ERR, "Network interface %s does not exist", _interface.c_str());
        return false;
    }
    
    _last_stats = read_network_stats();
    // Don't require non-zero bytes for initialization - interface might be idle
    _initialized = (_last_stats.rx_bytes != UINT64_MAX && _last_stats.tx_bytes != UINT64_MAX);
    
    if (_initialized) {
        syslog(LOG_INFO, "Bandwidth monitor initialized for interface %s (capacity: %u Mbps, initial: RX=%lu, TX=%lu)",
               _interface.c_str(), _capacity_mbps, _last_stats.rx_bytes, _last_stats.tx_bytes);
    } else {
        syslog(LOG_ERR, "Failed to read initial stats for interface %s", _interface.c_str());
    }
    
    return _initialized;
}

bandwidth_info_t bandwidth_monitor_t::get_bandwidth_usage() {
    bandwidth_info_t result = {0.0, 0.0, 0.0, 0.0, false};
    
    if (!_initialized) {
        return result;
    }
    
    network_stats_t current_stats = read_network_stats();
    if (current_stats.rx_bytes == UINT64_MAX || current_stats.tx_bytes == UINT64_MAX) {
        return result;
    }
    
    // Calculate time difference in seconds
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        current_stats.timestamp - _last_stats.timestamp).count();
    
    // Require at least 100ms between measurements to avoid division by zero
    // and ensure meaningful bandwidth calculations
    if (time_diff < 100) {
        return result;
    }
    
    double seconds = time_diff / 1000.0;
    
    // Calculate byte differences (handle counter wraparound)
    uint64_t rx_diff = (current_stats.rx_bytes >= _last_stats.rx_bytes) ?
        current_stats.rx_bytes - _last_stats.rx_bytes :
        (UINT64_MAX - _last_stats.rx_bytes) + current_stats.rx_bytes;
        
    uint64_t tx_diff = (current_stats.tx_bytes >= _last_stats.tx_bytes) ?
        current_stats.tx_bytes - _last_stats.tx_bytes :
        (UINT64_MAX - _last_stats.tx_bytes) + current_stats.tx_bytes;
    
    // Convert to Mbps (bytes/sec * 8 bits/byte / 1,000,000 bits/Mbps)
    result.rx_mbps = (rx_diff * 8.0) / (seconds * 1000000.0);
    result.tx_mbps = (tx_diff * 8.0) / (seconds * 1000000.0);
    result.total_mbps = result.rx_mbps + result.tx_mbps;
    
    // Calculate usage percentage
    result.usage_percentage = (result.total_mbps / _capacity_mbps) * 100.0;
    if (result.usage_percentage > 100.0) {
        result.usage_percentage = 100.0;
    }
    
    result.valid = true;
    
    // Update last stats for next calculation
    _last_stats = current_stats;
    
    return result;
}

network_stats_t bandwidth_monitor_t::read_network_stats() {
    network_stats_t stats = {UINT64_MAX, UINT64_MAX, std::chrono::steady_clock::now()};
    
    // Try /sys/class/net first (more reliable)
    if (parse_sys_class_net(_interface, stats)) {
        return stats;
    }
    
    // Fallback to /proc/net/dev
    if (parse_proc_net_dev(_interface, stats)) {
        return stats;
    }
    
    syslog(LOG_WARNING, "Failed to read network stats for interface %s", _interface.c_str());
    return stats;
}

bool bandwidth_monitor_t::parse_sys_class_net(const std::string& interface, network_stats_t& stats) {
    std::string rx_path = "/sys/class/net/" + interface + "/statistics/rx_bytes";
    std::string tx_path = "/sys/class/net/" + interface + "/statistics/tx_bytes";
    
    std::ifstream rx_file(rx_path);
    std::ifstream tx_file(tx_path);
    
    if (!rx_file.is_open() || !tx_file.is_open()) {
        return false;
    }
    
    std::string rx_str, tx_str;
    if (!std::getline(rx_file, rx_str) || !std::getline(tx_file, tx_str)) {
        return false;
    }
    
    try {
        stats.rx_bytes = std::stoull(rx_str);
        stats.tx_bytes = std::stoull(tx_str);
        stats.timestamp = std::chrono::steady_clock::now();
        return true;
    } catch (const std::exception& e) {
        syslog(LOG_WARNING, "Failed to parse network stats from sysfs: %s", e.what());
        return false;
    }
}

bool bandwidth_monitor_t::parse_proc_net_dev(const std::string& interface, network_stats_t& stats) {
    std::ifstream file("/proc/net/dev");
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    // Skip header lines
    std::getline(file, line);
    std::getline(file, line);
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string iface_name;
        
        // Extract interface name (remove colon)
        if (!(iss >> iface_name)) continue;
        if (iface_name.back() == ':') {
            iface_name.pop_back();
        }
        
        if (iface_name != interface) continue;
        
        // Parse the stats (rx_bytes is first, tx_bytes is 9th field after interface name)
        uint64_t rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
        uint64_t tx_bytes;
        
        if (iss >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo >> rx_frame >> rx_compressed >> rx_multicast >> tx_bytes) {
            stats.rx_bytes = rx_bytes;
            stats.tx_bytes = tx_bytes;
            stats.timestamp = std::chrono::steady_clock::now();
            return true;
        }
    }
    
    return false;
}