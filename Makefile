# LED Control Service Makefile

# Project configuration
PROJECT = ugreen_leds_ethutild
VERSION = 1.0.0

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Isrc
CXXFLAGS_DEBUG = -g -O0 -DDEBUG
CXXFLAGS_RELEASE = -O2 -DNDEBUG

# Default to release build
BUILD_TYPE ?= release
ifeq ($(BUILD_TYPE),debug)
    CXXFLAGS += $(CXXFLAGS_DEBUG)
else
    CXXFLAGS += $(CXXFLAGS_RELEASE)
endif

# Libraries
LIBS = -lpthread
# Try to link with i2c library if available
ifneq ($(shell pkg-config --exists libi2c 2>/dev/null; echo $$?),0)
    ifneq ($(wildcard /usr/lib/libi2c.so /usr/lib/x86_64-linux-gnu/libi2c.so),)
        LIBS += -li2c
    endif
endif

# Directories
SRCDIR = src
OBJDIR = obj
CONFIGDIR = config
SYSTEMDDIR = systemd

# Installation paths
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
CONFDIR = /etc
SYSTEMD_DIR = /etc/systemd/system

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
HEADERS = $(wildcard $(SRCDIR)/*.h)

# Target binary
TARGET = $(PROJECT)

# Default target
.PHONY: all
all: $(TARGET)

# Create object directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LIBS)

# Debug build
.PHONY: debug
debug:
	$(MAKE) BUILD_TYPE=debug

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(OBJDIR)
	rm -f $(TARGET)

# Install files and enable service
.PHONY: install
install: $(TARGET)
	@echo "Installing $(PROJECT)..."
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/
	install -d $(DESTDIR)$(CONFDIR)
	@if [ -f "$(DESTDIR)$(CONFDIR)/ugreen_leds_ethutild.conf" ]; then \
		echo "Existing config found at $(DESTDIR)$(CONFDIR)/ugreen_leds_ethutild.conf - preserving user configuration"; \
	else \
		echo "Installing default config to $(DESTDIR)$(CONFDIR)/ugreen_leds_ethutild.conf"; \
		install -m 644 $(CONFIGDIR)/ugreen_leds_ethutild.conf $(DESTDIR)$(CONFDIR)/; \
	fi
	install -d $(DESTDIR)$(SYSTEMD_DIR)
	install -m 644 $(SYSTEMDDIR)/ugreen_leds_ethutild.service $(DESTDIR)$(SYSTEMD_DIR)/
	@echo "Enabling and starting service..."
	systemctl daemon-reload
	systemctl enable ugreen_leds_ethutild.service
	systemctl start ugreen_leds_ethutild.service
	@echo "Installation complete and service started."

# Stop, disable and uninstall service
.PHONY: uninstall
uninstall:
	@echo "Stopping and disabling service..."
	-systemctl stop ugreen_leds_ethutild.service
	-systemctl disable ugreen_leds_ethutild.service
	@echo "Uninstalling $(PROJECT)..."
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(CONFDIR)/ugreen_leds_ethutild.conf
	rm -f $(DESTDIR)$(SYSTEMD_DIR)/ugreen_leds_ethutild.service
	systemctl daemon-reload
	@echo "Service stopped, disabled and uninstalled."

# Show help
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all              - Build the project (default)"
	@echo "  debug            - Build with debug flags"
	@echo "  clean            - Remove build artifacts"
	@echo "  install          - Install binary, config, systemd service and start service"
	@echo "  uninstall        - Stop service and remove all installed files"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Build options:"
	@echo "  BUILD_TYPE=debug   - Build with debug symbols"
	@echo "  BUILD_TYPE=release - Build optimized (default)"
	@echo "  PREFIX=/path       - Set installation prefix (default: /usr/local)"
	@echo ""
	@echo "Service management:"
	@echo "  sudo systemctl status ugreen_leds_ethutild   - Check service status"
	@echo "  sudo systemctl stop ugreen_leds_ethutild     - Stop the service"
	@echo "  sudo systemctl start ugreen_leds_ethutild    - Start the service"
	@echo "  sudo ugreen_leds_ethutild --test             - Run in testing mode"

# Show project info
.PHONY: info
info:
	@echo "Project: $(PROJECT) v$(VERSION)"
	@echo "Build type: $(BUILD_TYPE)"
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(CXXFLAGS)"
	@echo "Libraries: $(LIBS)"
	@echo "Sources: $(SOURCES)"
	@echo "Install prefix: $(PREFIX)"