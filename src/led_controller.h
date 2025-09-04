#ifndef __LEDCTL_LED_CONTROLLER_H__
#define __LEDCTL_LED_CONTROLLER_H__

#include <array>
#include <optional>

#include "i2c.h"

// LED type definitions
#define LEDCTL_LED_POWER    led_controller_t::led_type_t::power
#define LEDCTL_LED_NETDEV   led_controller_t::led_type_t::netdev
#define LEDCTL_LED_DISK1    led_controller_t::led_type_t::disk1
#define LEDCTL_LED_DISK2    led_controller_t::led_type_t::disk2

#define LEDCTL_LED_I2C_ADDR  0x3a

// Color constants
struct rgb_color_t {
    uint8_t r, g, b;
};

// Predefined colors
const rgb_color_t COLOR_WHITE = {255, 255, 255};
const rgb_color_t COLOR_GREEN = {0, 255, 0};
const rgb_color_t COLOR_BLUE = {0, 0, 255};
const rgb_color_t COLOR_RED = {255, 0, 0};
const rgb_color_t COLOR_OFF = {0, 0, 0};

// Default brightness
const uint8_t DEFAULT_BRIGHTNESS = 255;

class led_controller_t {

    i2c_device_t _i2c;

public:

    enum class op_mode_t : uint8_t {
        off = 0, on, blink, breath
    };

    enum class led_type_t : uint8_t {
        power = 0, netdev, disk1, disk2, disk3, disk4, disk5, disk6, disk7, disk8
    };

    struct led_data_t {
        bool is_available;
        op_mode_t op_mode;
        uint8_t brightness;
        uint8_t color_r, color_g, color_b;
        uint16_t t_on, t_off;
    };

public:
    int start();
    
    // High-level interface for the service
    int set_led_state(led_type_t id, bool on, const rgb_color_t& color = COLOR_WHITE, uint8_t brightness = DEFAULT_BRIGHTNESS);
    int turn_off_led(led_type_t id);
    int turn_off_all_leds();
    
    // Low-level interface (from reference code)
    led_data_t get_status(led_type_t id);
    int set_onoff(led_type_t id, uint8_t status);
    int set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b);
    int set_brightness(led_type_t id, uint8_t brightness);
    int set_blink(led_type_t id, uint16_t t_on, uint16_t t_off);
    int set_breath(led_type_t id, uint16_t t_on, uint16_t t_off);

    bool is_last_modification_successful();

private:
    int _set_blink_or_breath(uint8_t command, led_type_t id, uint16_t t_on, uint16_t t_off);
    int _change_status(led_type_t id, uint8_t command, std::array<std::optional<uint8_t>, 4> params);
};

#endif