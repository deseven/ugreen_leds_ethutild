#ifndef __LEDCTL_CONFIG_PARSER_H__
#define __LEDCTL_CONFIG_PARSER_H__

#include <string>
#include <map>

struct ledctl_config_t {
    // Network settings
    std::string interface;
    uint32_t capacity_mbps;
    
    // LED settings
    uint8_t brightness;
    
    // Logging settings
    std::string log_level;
    
    // Default values
    ledctl_config_t() 
        : interface("eth0")
        , capacity_mbps(2000)  // 1Gbps full duplex
        , brightness(255)
        , log_level("info")
    {}
};

class config_parser_t {
private:
    std::map<std::string, std::string> _config_data;
    
    bool parse_file(const std::string& filename);
    std::string trim(const std::string& str);
    std::string get_value(const std::string& section, const std::string& key, const std::string& default_value = "");
    
public:
    // Load configuration from file
    // Tries ./ugreen_leds_ethutild.conf first, then /etc/ugreen_leds_ethutild.conf
    bool load_config(ledctl_config_t& config);
    
    // Load configuration from specific file
    bool load_config_from_file(const std::string& filename, ledctl_config_t& config);
    
    // Create example configuration file
    static bool create_example_config(const std::string& filename);
};

#endif