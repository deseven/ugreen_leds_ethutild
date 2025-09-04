#include "config_parser.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <syslog.h>

bool config_parser_t::load_config(ledctl_config_t& config) {
    // Try local config first
    if (std::filesystem::exists("./ugreen_leds_ethutild.conf")) {
        if (load_config_from_file("./ugreen_leds_ethutild.conf", config)) {
            syslog(LOG_INFO, "Loaded configuration from ./ugreen_leds_ethutild.conf");
            return true;
        }
    }
    
    // Try system config
    if (std::filesystem::exists("/etc/ugreen_leds_ethutild.conf")) {
        if (load_config_from_file("/etc/ugreen_leds_ethutild.conf", config)) {
            syslog(LOG_INFO, "Loaded configuration from /etc/ugreen_leds_ethutild.conf");
            return true;
        }
    }
    
    syslog(LOG_INFO, "No configuration file found, using defaults");
    return true; // Use defaults
}

bool config_parser_t::load_config_from_file(const std::string& filename, ledctl_config_t& config) {
    if (!parse_file(filename)) {
        syslog(LOG_ERR, "Failed to parse configuration file: %s", filename.c_str());
        return false;
    }
    
    // Parse network settings
    std::string interface = get_value("network", "interface", config.interface);
    if (!interface.empty()) {
        config.interface = interface;
    }
    
    std::string capacity_str = get_value("network", "capacity_mbps");
    if (!capacity_str.empty()) {
        try {
            config.capacity_mbps = std::stoul(capacity_str);
        } catch (const std::exception& e) {
            syslog(LOG_WARNING, "Invalid capacity_mbps value: %s, using default", capacity_str.c_str());
        }
    }
    
    // Parse LED settings
    std::string brightness_str = get_value("leds", "brightness");
    if (!brightness_str.empty()) {
        try {
            int brightness = std::stoi(brightness_str);
            if (brightness >= 0 && brightness <= 255) {
                config.brightness = static_cast<uint8_t>(brightness);
            } else {
                syslog(LOG_WARNING, "Brightness value out of range (0-255): %d, using default", brightness);
            }
        } catch (const std::exception& e) {
            syslog(LOG_WARNING, "Invalid brightness value: %s, using default", brightness_str.c_str());
        }
    }
    
    // Parse threshold settings
    std::string low_threshold_str = get_value("leds", "low_threshold");
    if (!low_threshold_str.empty()) {
        try {
            int threshold = std::stoi(low_threshold_str);
            if (threshold >= 0 && threshold <= 100) {
                config.low_threshold = static_cast<uint8_t>(threshold);
            } else {
                syslog(LOG_WARNING, "Low threshold value out of range (0-100): %d, using default", threshold);
            }
        } catch (const std::exception& e) {
            syslog(LOG_WARNING, "Invalid low_threshold value: %s, using default", low_threshold_str.c_str());
        }
    }
    
    std::string medium_threshold_str = get_value("leds", "medium_threshold");
    if (!medium_threshold_str.empty()) {
        try {
            int threshold = std::stoi(medium_threshold_str);
            if (threshold >= 0 && threshold <= 100) {
                config.medium_threshold = static_cast<uint8_t>(threshold);
            } else {
                syslog(LOG_WARNING, "Medium threshold value out of range (0-100): %d, using default", threshold);
            }
        } catch (const std::exception& e) {
            syslog(LOG_WARNING, "Invalid medium_threshold value: %s, using default", medium_threshold_str.c_str());
        }
    }
    
    std::string high_threshold_str = get_value("leds", "high_threshold");
    if (!high_threshold_str.empty()) {
        try {
            int threshold = std::stoi(high_threshold_str);
            if (threshold >= 0 && threshold <= 100) {
                config.high_threshold = static_cast<uint8_t>(threshold);
            } else {
                syslog(LOG_WARNING, "High threshold value out of range (0-100): %d, using default", threshold);
            }
        } catch (const std::exception& e) {
            syslog(LOG_WARNING, "Invalid high_threshold value: %s, using default", high_threshold_str.c_str());
        }
    }
    
    // Parse logging settings
    std::string log_level = get_value("logging", "level", config.log_level);
    if (!log_level.empty()) {
        config.log_level = log_level;
    }
    
    return true;
}

bool config_parser_t::parse_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    _config_data.clear();
    std::string line;
    std::string current_section;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Check for section header
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            current_section = trim(current_section);
            continue;
        }
        
        // Parse key=value pairs
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(line.substr(0, eq_pos));
            std::string value = trim(line.substr(eq_pos + 1));
            
            // Remove quotes if present
            if (value.length() >= 2 && 
                ((value.front() == '"' && value.back() == '"') ||
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.length() - 2);
            }
            
            std::string full_key = current_section.empty() ? key : current_section + "." + key;
            _config_data[full_key] = value;
        }
    }
    
    return true;
}

std::string config_parser_t::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string config_parser_t::get_value(const std::string& section, const std::string& key, const std::string& default_value) {
    std::string full_key = section + "." + key;
    auto it = _config_data.find(full_key);
    if (it != _config_data.end()) {
        return it->second;
    }
    return default_value;
}

bool config_parser_t::create_example_config(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << "[network]\n";
    file << "interface = eth0\n";
    file << "capacity_mbps = 2000\n\n";
    
    file << "[leds]\n";
    file << "brightness = 255\n";
    file << "low_threshold = 10\n";
    file << "medium_threshold = 40\n";
    file << "high_threshold = 80\n\n";
    
    file << "[logging]\n";
    file << "level = info\n";
    
    return true;
}