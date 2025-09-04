#include "led_controller.h"
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <syslog.h>
#include <unistd.h>

#define I2C_DEV_PATH  "/sys/class/i2c-dev/"

int led_controller_t::start() {
    namespace fs = std::filesystem;

    if (!fs::exists(I2C_DEV_PATH)) {
        syslog(LOG_ERR, "I2C device path %s does not exist", I2C_DEV_PATH);
        return -1;
    }

    for (const auto& entry : fs::directory_iterator(I2C_DEV_PATH)) {
        if (entry.is_directory()) {
            std::ifstream ifs(entry.path() / "device/name");
            std::string line;
            std::getline(ifs, line);

            if (line.rfind("SMBus I801 adapter", 0) == 0) {
                const auto i2c_dev = "/dev/" + entry.path().filename().string();
                int result = _i2c.start(i2c_dev.c_str(), LEDCTL_LED_I2C_ADDR);
                if (result == 0) {
                    syslog(LOG_INFO, "LED controller initialized on %s", i2c_dev.c_str());
                } else {
                    syslog(LOG_ERR, "Failed to initialize LED controller on %s", i2c_dev.c_str());
                }
                return result;
            }
        }
    }

    syslog(LOG_ERR, "No compatible I2C adapter found");
    return -1;
}

static int compute_checksum(const std::vector<uint8_t>& data, int size) {
    if (size < 2 || size > (int)data.size()) 
        return 0;

    int sum = 0;
    for (int i = 0; i < size; ++i)
        sum += (int)data[i];

    return sum;
}

static bool verify_checksum(const std::vector<uint8_t>& data) {
    int size = data.size();
    if (size < 2) return false;
    int sum = compute_checksum(data, size - 2);
    return sum != 0 && sum == (data[size - 1] | (((int)data[size - 2]) << 8));
}

static void append_checksum(std::vector<uint8_t>& data) {
    int size = data.size();
    int sum = compute_checksum(data, size);
    data.push_back((sum >> 8) & 0xff);
    data.push_back(sum & 0xff);
}

// High-level interface methods
int led_controller_t::set_led_state(led_type_t id, bool on, const rgb_color_t& color, uint8_t brightness) {
    if (!on) {
        return turn_off_led(id);
    }
    
    int result = 0;
    
    // Set color first
    result = set_rgb(id, color.r, color.g, color.b);
    if (result != 0) {
        syslog(LOG_ERR, "Failed to set RGB for LED %d", (int)id);
        return result;
    }
    
    // Small delay between operations
    usleep(10000); // 10ms
    
    // Set brightness
    result = set_brightness(id, brightness);
    if (result != 0) {
        syslog(LOG_ERR, "Failed to set brightness for LED %d", (int)id);
        return result;
    }
    
    // Small delay between operations
    usleep(10000); // 10ms
    
    // Turn on
    result = set_onoff(id, 1);
    if (result != 0) {
        syslog(LOG_ERR, "Failed to turn on LED %d", (int)id);
    }
    
    return result;
}

int led_controller_t::turn_off_led(led_type_t id) {
    return set_onoff(id, 0);
}

int led_controller_t::turn_off_all_leds() {
    int result = 0;
    
    // Turn off each LED individually with delays and error checking
    result = turn_off_led(led_type_t::power);
    if (result != 0) {
        syslog(LOG_ERR, "Failed to turn off power LED");
    }
    usleep(20000); // 20ms delay
    
    int temp_result = turn_off_led(led_type_t::netdev);
    if (temp_result != 0) {
        syslog(LOG_ERR, "Failed to turn off netdev LED");
        result |= temp_result;
    }
    usleep(20000);
    
    temp_result = turn_off_led(led_type_t::disk1);
    if (temp_result != 0) {
        syslog(LOG_ERR, "Failed to turn off disk1 LED");
        result |= temp_result;
    }
    usleep(20000);
    
    temp_result = turn_off_led(led_type_t::disk2);
    if (temp_result != 0) {
        syslog(LOG_ERR, "Failed to turn off disk2 LED");
        result |= temp_result;
    }
    
    // Additional verification - try to verify the operation was successful
    if (result == 0) {
        usleep(50000); // Wait 50ms for operations to complete
        if (!is_last_modification_successful()) {
            syslog(LOG_WARNING, "LED controller reports last modification was not successful");
            result = -1;
        }
    }
    
    return result;
}

// Low-level interface methods (from reference code)
led_controller_t::led_data_t led_controller_t::get_status(led_type_t id) {
    led_data_t data { };
    data.is_available = false;

    auto raw_data = _i2c.read_block_data(0x81 + (uint8_t)id, 0xb);
    if (raw_data.size() != 0xb || !verify_checksum(raw_data)) 
        return data;

    switch (raw_data[0]) {
        case 0: data.op_mode = op_mode_t::off; break;
        case 1: data.op_mode = op_mode_t::on; break;
        case 2: data.op_mode = op_mode_t::blink; break;
        case 3: data.op_mode = op_mode_t::breath; break;
        default: return data;
    };

    data.brightness = raw_data[1];
    data.color_r = raw_data[2];
    data.color_g = raw_data[3];
    data.color_b = raw_data[4];
    int t_hight = (((int)raw_data[5]) << 8) | raw_data[6];
    int t_low = (((int)raw_data[7]) << 8) | raw_data[8];
    data.t_on = t_low;
    data.t_off = t_hight - t_low;
    data.is_available = true;

    return data;
}

int led_controller_t::_change_status(led_type_t id, uint8_t command, std::array<std::optional<uint8_t>, 4> params) {
    std::vector<uint8_t> data {
    //   3c    3b    3a
        0x00, 0xa0, 0x01,
    //     39        38         37
        0x00, 0x00, command, 
    //     36 - 33
        params[0].value_or(0x00), 
        params[1].value_or(0x00), 
        params[2].value_or(0x00), 
        params[3].value_or(0x00), 
    };

    append_checksum(data);
    data[0] = (uint8_t)id;
    return _i2c.write_block_data((uint8_t)id, data);
}

int led_controller_t::set_onoff(led_type_t id, uint8_t status) {
    if (status >= 2) return -1;
    return _change_status(id, 0x03, { status } );
}

int led_controller_t::_set_blink_or_breath(uint8_t command, led_type_t id, uint16_t t_on, uint16_t t_off) {
    uint16_t t_hight = t_on + t_off;
    uint16_t t_low = t_on;
    return _change_status(id, command, { 
        (uint8_t)(t_hight >> 8), 
        (uint8_t)(t_hight & 0xff), 
        (uint8_t)(t_low >> 8),
        (uint8_t)(t_low & 0xff),
    } );
}

int led_controller_t::set_rgb(led_type_t id, uint8_t r, uint8_t g, uint8_t b) {
    return _change_status(id, 0x02, { r, g, b } );
}

int led_controller_t::set_brightness(led_type_t id, uint8_t brightness) {
    return _change_status(id, 0x01, { brightness } );
}

bool led_controller_t::is_last_modification_successful() {
    return _i2c.read_byte_data(0x80) == 1;
}

int led_controller_t::set_blink(led_type_t id, uint16_t t_on, uint16_t t_off) {
    return _set_blink_or_breath(0x04, id, t_on, t_off);
}

int led_controller_t::set_breath(led_type_t id, uint16_t t_on, uint16_t t_off) {
    return _set_blink_or_breath(0x05, id, t_on, t_off);
}