#ifndef __LEDCTL_BANDWIDTH_MONITOR_H__
#define __LEDCTL_BANDWIDTH_MONITOR_H__

#include <string>
#include <chrono>

struct network_stats_t {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    std::chrono::steady_clock::time_point timestamp;
};

struct bandwidth_info_t {
    double rx_mbps;
    double tx_mbps;
    double total_mbps;
    double usage_percentage;
    bool valid;
};

class bandwidth_monitor_t {
private:
    std::string _interface;
    uint32_t _capacity_mbps;
    network_stats_t _last_stats;
    bool _initialized;

    network_stats_t read_network_stats();
    bool parse_proc_net_dev(const std::string& interface, network_stats_t& stats);
    bool parse_sys_class_net(const std::string& interface, network_stats_t& stats);

public:
    bandwidth_monitor_t(const std::string& interface, uint32_t capacity_mbps);
    
    // Initialize the monitor (takes first measurement)
    bool initialize();
    
    // Get current bandwidth usage
    bandwidth_info_t get_bandwidth_usage();
    
    // Get interface name
    const std::string& get_interface() const { return _interface; }
    
    // Get capacity
    uint32_t get_capacity_mbps() const { return _capacity_mbps; }
};

#endif