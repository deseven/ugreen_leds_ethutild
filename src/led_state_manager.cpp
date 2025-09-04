#include "led_state_manager.h"
#include <syslog.h>
#include <unistd.h>

led_state_manager_t::led_state_manager_t(led_controller_t& led_controller, const ledctl_config_t& config)
    : _led_controller(led_controller), _current_state(led_state_t::UTILIZATION_OFF),
      _brightness(config.brightness), _low_threshold(config.low_threshold),
      _medium_threshold(config.medium_threshold), _high_threshold(config.high_threshold) {
}

bool led_state_manager_t::update_leds(const bandwidth_info_t& bandwidth_info) {
    if (!bandwidth_info.valid) {
        syslog(LOG_WARNING, "Invalid bandwidth info, keeping current LED state");
        return false;
    }
    
    led_state_t new_state = determine_state_from_usage(bandwidth_info.usage_percentage);
    
    // Only update if state changed
    if (new_state != _current_state) {
        syslog(LOG_INFO, "Bandwidth usage: %.1f%% (%.1f Mbps) - changing LED state from %s to %s",
               bandwidth_info.usage_percentage, bandwidth_info.total_mbps,
               get_state_name(_current_state), get_state_name(new_state));
        
        if (apply_led_state(new_state)) {
            _current_state = new_state;
            return true;
        } else {
            syslog(LOG_ERR, "Failed to apply LED state: %s", get_state_name(new_state));
            return false;
        }
    }
    
    return true; // No change needed, but not an error
}

bool led_state_manager_t::set_state(led_state_t state) {
    if (apply_led_state(state)) {
        _current_state = state;
        return true;
    }
    return false;
}

led_state_t led_state_manager_t::determine_state_from_usage(double usage_percentage) {
    if (usage_percentage < _low_threshold) {
        return led_state_t::UTILIZATION_OFF;
    } else if (usage_percentage < _medium_threshold) {
        return led_state_t::NETDEV_GREEN;
    } else if (usage_percentage < _high_threshold) {
        return led_state_t::NETDEV_DISK1_BLUE;
    } else {
        return led_state_t::ALL_UTILIZATION_RED;
    }
}

bool led_state_manager_t::apply_led_state(led_state_t state) {
    // Get target LED states for the new state
    bool target_netdev_on, target_disk1_on, target_disk2_on;
    rgb_color_t target_color;
    get_target_led_states(state, target_netdev_on, target_disk1_on, target_disk2_on, target_color);
    
    syslog(LOG_DEBUG, "Applying LED state %s: netdev=%s, disk1=%s, disk2=%s, color=(%d,%d,%d)",
           get_state_name(state),
           target_netdev_on ? "on" : "off",
           target_disk1_on ? "on" : "off",
           target_disk2_on ? "on" : "off",
           target_color.r, target_color.g, target_color.b);
    
    // Always ensure power LED is on and white
    int result = _led_controller.set_led_state(LEDCTL_LED_POWER, true, COLOR_WHITE, _brightness);
    if (result != 0) {
        syslog(LOG_ERR, "Failed to set power LED");
        return false;
    }
    usleep(100000); // 100ms delay after power LED
    
    // Apply utilization LEDs with incremental changes
    bool success = true;
    
    // Handle netdev LED
    if (target_netdev_on) {
        result = _led_controller.set_led_state(LEDCTL_LED_NETDEV, true, target_color, _brightness);
        if (result != 0) {
            syslog(LOG_ERR, "Failed to set netdev LED");
            success = false;
        }
    } else {
        result = _led_controller.turn_off_led(LEDCTL_LED_NETDEV);
        if (result != 0) {
            syslog(LOG_ERR, "Failed to turn off netdev LED");
            success = false;
        }
    }
    usleep(100000); // 100ms delay between LEDs
    
    // Handle disk1 LED
    if (target_disk1_on) {
        result = _led_controller.set_led_state(LEDCTL_LED_DISK1, true, target_color, _brightness);
        if (result != 0) {
            syslog(LOG_ERR, "Failed to set disk1 LED");
            success = false;
        }
    } else {
        result = _led_controller.turn_off_led(LEDCTL_LED_DISK1);
        if (result != 0) {
            syslog(LOG_ERR, "Failed to turn off disk1 LED");
            success = false;
        }
    }
    usleep(100000); // 100ms delay between LEDs
    
    // Handle disk2 LED
    if (target_disk2_on) {
        result = _led_controller.set_led_state(LEDCTL_LED_DISK2, true, target_color, _brightness);
        if (result != 0) {
            syslog(LOG_ERR, "Failed to set disk2 LED");
            success = false;
        }
    } else {
        result = _led_controller.turn_off_led(LEDCTL_LED_DISK2);
        if (result != 0) {
            syslog(LOG_ERR, "Failed to turn off disk2 LED");
            success = false;
        }
    }
    
    if (!success) {
        syslog(LOG_ERR, "Failed to apply LED state %s", get_state_name(state));
        return false;
    }
    
    syslog(LOG_DEBUG, "Successfully applied LED state %s", get_state_name(state));
    return true;
}

void led_state_manager_t::get_target_led_states(led_state_t state, bool& netdev_on, bool& disk1_on, bool& disk2_on, rgb_color_t& color) {
    switch (state) {
        case led_state_t::UTILIZATION_OFF:
            netdev_on = false;
            disk1_on = false;
            disk2_on = false;
            color = COLOR_OFF;
            break;
            
        case led_state_t::NETDEV_GREEN:
            netdev_on = true;
            disk1_on = false;
            disk2_on = false;
            color = COLOR_GREEN;
            break;
            
        case led_state_t::NETDEV_DISK1_BLUE:
            netdev_on = true;
            disk1_on = true;
            disk2_on = false;
            color = COLOR_BLUE;
            break;
            
        case led_state_t::ALL_UTILIZATION_RED:
            netdev_on = true;
            disk1_on = true;
            disk2_on = true;
            color = COLOR_RED;
            break;
    }
}

const char* led_state_manager_t::get_state_name(led_state_t state) {
    switch (state) {
        case led_state_t::UTILIZATION_OFF:
            return "UTILIZATION_OFF";
        case led_state_t::NETDEV_GREEN:
            return "NETDEV_GREEN";
        case led_state_t::NETDEV_DISK1_BLUE:
            return "NETDEV_DISK1_BLUE";
        case led_state_t::ALL_UTILIZATION_RED:
            return "ALL_UTILIZATION_RED";
        default:
            return "UNKNOWN";
    }
}