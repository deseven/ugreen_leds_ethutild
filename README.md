# UGREEN LEDs Ethernet Utilization Daemon (ugreen_leds_ethutild)

A C++ service that monitors network bandwidth usage and controls UGREEN DXP2800 NAS LEDs based on ethernet traffic levels. Thanks to [miskcoo/ugreen_leds_controller](https://github.com/miskcoo/ugreen_leds_controller) for showing how it could be done.


## LED Behavior

The service controls LEDs based on bandwidth utilization percentage:

**Power LED**: Always on and white when the service is running (indicates service status)

**Utilization LEDs** (based on bandwidth usage):

| Usage Range | Utilization LEDs Active | Color | Description |
|-------------|-------------------------|-------|-------------|
| 0-low_threshold% | None | Off | Low utilization |
| low_threshold-medium_threshold% | NetDev | Green | Moderate utilization |
| medium_threshold-high_threshold% | NetDev + Disk1 | Blue | High utilization |
| high_threshold%+ | NetDev + Disk1 + Disk2 | Red | Very high utilization |

*Default thresholds: 10%, 40%, 80% (configurable via config file)*


## Requirements

- Linux system with I2C support
- Compatible UGREEN NAS hardware
- Root privileges (for I2C access)
- Make build system
- C++17 compatible compiler (g++)
- i2c-dev kernel module loaded


## Installation

```bash
git clone ...
make
sudo make install
```

The `install` target will automatically:
- Install the binary to `/usr/local/bin/`
- Install the configuration file to `/etc/ugreen_leds_ethutild.conf` (preserving existing config)
- Install the systemd service file
- Enable and start the service


## Configuration

The service looks for configuration files in this order:
1. `./ugreen_leds_ethutild.conf` (current directory)
2. `/etc/ugreen_leds_ethutild.conf` (system-wide)

Current config options:

**Network settings:**
- **interface**: Network interface to monitor (e.g., `eth0`, `enp2s0`)
- **capacity_mbps**: Total link capacity in Mbps (full duplex)
  - 1Gbps full duplex = 2000 Mbps
  - 10Gbps full duplex = 20000 Mbps
  - you can also define total bandwidth as max throughput of your disks

**LED settings:**
- **brightness**: LED brightness (0-255)
- **low_threshold**: Percentage threshold for low utilization (default: 10)
- **medium_threshold**: Percentage threshold for medium utilization (default: 40)
- **high_threshold**: Percentage threshold for high utilization (default: 80)

**Logging settings:**
- **level**: Log level (`debug`, `info`, `warning`, `error`)


## Usage

```bash
# Testing mode (cycles through all LED states)
sudo ugreen_leds_ethutild --test

# Service control
sudo systemctl start/stop/status ugreen_leds_ethutild

# View logs
sudo journalctl -fu ugreen_leds_ethutild
```