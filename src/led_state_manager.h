#ifndef __LEDCTL_LED_STATE_MANAGER_H__
#define __LEDCTL_LED_STATE_MANAGER_H__

#include "led_controller.h"
#include "bandwidth_monitor.h"
#include "config_parser.h"

enum class led_state_t {
    UTILIZATION_OFF,        // idle: netdev, disk1, disk2 off (power always on)
    NETDEV_GREEN,          // low: netdev green
    NETDEV_DISK1_BLUE,     // medium: netdev and disk1 blue
    ALL_UTILIZATION_RED    // high: netdev, disk1, disk2 red
};

class led_state_manager_t {
private:
    led_controller_t& _led_controller;
    led_state_t _current_state;
    uint8_t _brightness;
    uint8_t _low_threshold;
    uint8_t _medium_threshold;
    uint8_t _high_threshold;
    
    // Core logic methods
    led_state_t determine_state_from_usage(double usage_percentage);
    bool apply_led_state(led_state_t state);
    
    // Helper methods for LED control
    void get_target_led_states(led_state_t state, bool& netdev_on, bool& disk1_on, bool& disk2_on, rgb_color_t& color);
    
public:
    led_state_manager_t(led_controller_t& led_controller, const ledctl_config_t& config);
    
    // Update LEDs based on bandwidth usage
    bool update_leds(const bandwidth_info_t& bandwidth_info);
    
    // Set LEDs to specific state (for testing)
    bool set_state(led_state_t state);
    
    // Get current state
    led_state_t get_current_state() const { return _current_state; }
    
    // Get state name for logging
    static const char* get_state_name(led_state_t state);
};

#endif