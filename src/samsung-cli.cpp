#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h> // For getopt
#include <sys/stat.h>
#include <map>
#include <memory>
#include <functional>
#include <dirent.h>

const std::string POWER_PATH = "/sys/class/power_supply/BAT1/charge_control_end_threshold";
const std::string FAN_PATH = "/sys/bus/acpi/devices/PNP0C0B:00/fan_speed_rpm";
const std::string PLATFORM_PROFILE_PATH = "/sys/firmware/acpi/platform_profile";
const std::string PLATFORM_PROFILE_CHOICES_PATH = "/sys/firmware/acpi/platform_profile_choices";
const std::string KBD_BACKLIGHT_PATH = "/sys/class/leds/samsung-galaxybook::kbd_backlight/brightness";

// Note: The keyboard backlight is affected by:
// 1. Ambient light sensor (automatically adjusts based on lighting conditions)
// 2. GNOME's automatic backlight control (reduces brightness after idle)
// 3. Manual control through this tool (values 0-3)

// Function to detect the correct path for a feature
std::string detect_feature_path(const std::string& feature_name) {
    // Try the udev rule path first
    std::string udev_path = "/dev/samsung-galaxybook/" + feature_name;
    if (access(udev_path.c_str(), R_OK) == 0) {
        return udev_path;
    }
    
    // Try the platform driver path
    DIR* dir = opendir("/sys/bus/platform/drivers/samsung-galaxybook");
    if (dir != nullptr) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strncmp(entry->d_name, "SAM", 3) == 0) {
                std::string path = "/sys/bus/platform/drivers/samsung-galaxybook/" + std::string(entry->d_name) + "/" + feature_name;
                if (access(path.c_str(), R_OK) == 0) {
                    closedir(dir);
                    return path;
                }
            }
        }
        closedir(dir);
    }
    
    // If no path found, return the original ACPI path as fallback
    return "/sys/bus/acpi/devices/SCAI:00/" + feature_name;
}

// Global variables to store the detected paths
std::string ALLOW_RECORDING_PATH = detect_feature_path("allow_recording");
std::string START_ON_LID_OPEN_PATH = detect_feature_path("start_on_lid_open");
std::string USB_CHARGE_PATH = detect_feature_path("usb_charge");

// Base class for all commands
class Command {
public:
    virtual ~Command() = default;
    virtual bool execute(const std::vector<std::string>& args) = 0;
    virtual std::string get_help() const = 0;
};

// Helper functions for file operations
namespace file_ops {
    bool check_permissions(const std::string& path, bool write = false) {
        if (access(path.c_str(), write ? W_OK : R_OK) != 0) {
            std::cerr << "Error: Permission denied. Run with sudo." << std::endl;
            return false;
        }
        return true;
    }

    bool read_file(const std::string& path, std::string& value) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open " << path << std::endl;
            return false;
        }
        std::getline(file, value);
        file.close();
        return true;
    }

    bool write_file(const std::string& path, const std::string& value) {
        if (!check_permissions(path, true)) return false;
        
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "Error: Could not write to " << path << std::endl;
            return false;
        }
        file << value;
        file.close();
        return true;
    }
}

// Command implementations
class PowerCommand : public Command {
public:
    bool execute(const std::vector<std::string>& args) override {
        if (args.size() < 2) {
            std::cerr << "Error: Missing power subcommand. Use 'read' or 'set'." << std::endl;
            return false;
        }

        std::string subcommand = args[1];
        if (subcommand == "read") {
            return read_power();
        } else if (subcommand == "set") {
            if (args.size() < 3) {
                std::cerr << "Error: Missing value for 'power set'" << std::endl;
                return false;
            }
            return set_power(args[2]);
        }
        std::cerr << "Error: Unknown power subcommand '" << subcommand << "'" << std::endl;
        return false;
    }

    std::string get_help() const override {
        return "  power read    Read the charge threshold\n"
               "  power set <value>  Set the charge threshold (0-100)";
    }

private:
    bool read_power() {
        std::string value;
        if (!file_ops::read_file(POWER_PATH, value)) return false;
        std::cout << "Current charge threshold: " << value << "%" << std::endl;
        return true;
    }

    bool set_power(const std::string& value) {
        int val;
        try {
            val = std::stoi(value);
            if (val < 0 || val > 100) {
                std::cerr << "Error: Value must be between 0 and 100" << std::endl;
                return false;
            }
        } catch (...) {
            std::cerr << "Error: Invalid value '" << value << "'" << std::endl;
            return false;
        }
        if (!file_ops::write_file(POWER_PATH, std::to_string(val))) return false;
        std::cout << "Set charge threshold to " << val << "%" << std::endl;
        return true;
    }
};

class FanCommand : public Command {
public:
    bool execute(const std::vector<std::string>& args) override {
        if (args.size() < 2) {
            std::cerr << "Error: Missing fan subcommand. Use 'read'." << std::endl;
            return false;
        }

        std::string subcommand = args[1];  // args[0] is the command name "fan"
        if (subcommand == "read") {
            return read_fan();
        }
        std::cerr << "Error: Unknown fan subcommand '" << subcommand << "'" << std::endl;
        return false;
    }

    std::string get_help() const override {
        return "  fan read      Read current fan speed in RPM";
    }

private:
    bool read_fan() {
        std::string value;
        if (!file_ops::read_file(FAN_PATH, value)) return false;
        std::cout << "Current fan speed: " << value << " RPM" << std::endl;
        return true;
    }
};

class PerformanceCommand : public Command {
public:
    bool execute(const std::vector<std::string>& args) override {
        if (args.size() < 2) {
            std::cerr << "Error: Missing performance subcommand. Use 'read', 'set', or 'list'." << std::endl;
            return false;
        }

        std::string subcommand = args[1];
        if (subcommand == "read") {
            return read_performance_mode();
        } else if (subcommand == "set") {
            if (args.size() < 3) {
                std::cerr << "Error: Missing mode for 'perf set'" << std::endl;
                return false;
            }
            return set_performance_mode(args[2]);
        } else if (subcommand == "list") {
            return list_performance_modes();
        }
        std::cerr << "Error: Unknown performance subcommand '" << subcommand << "'" << std::endl;
        return false;
    }

    std::string get_help() const override {
        return "  perf read     Read current performance mode\n"
               "  perf set <mode>  Set performance mode (low-power/balanced/performance)\n"
               "  perf list     List available performance modes";
    }

private:
    bool read_performance_mode() {
        std::string value;
        if (!file_ops::read_file(PLATFORM_PROFILE_PATH, value)) return false;
        std::cout << "Current performance mode: " << value << std::endl;
        return true;
    }

    bool set_performance_mode(const std::string& mode) {
        // Check if the mode is valid. If 'mode' is not in the list of available modes, return false
        std::string available_modes;
        if (!file_ops::read_file(PLATFORM_PROFILE_CHOICES_PATH, available_modes)) return false;
        if (available_modes.find(mode) == std::string::npos) {
            std::cerr << "Error: Invalid performance mode '" << mode << "'" << std::endl;
            return false;
        }

        if (!file_ops::write_file(PLATFORM_PROFILE_PATH, mode)) return false;
        std::cout << "Set performance mode to " << mode << std::endl;
        return true;
    }

    bool list_performance_modes() {
        std::string value;
        if (!file_ops::read_file(PLATFORM_PROFILE_CHOICES_PATH, value)) return false;
        std::cout << "Available performance modes: " << value << std::endl;
        return true;
    }
};

class RecordingCommand : public Command {
public:
    bool execute(const std::vector<std::string>& args) override {
        if (args.size() < 2) {
            std::cerr << "Error: Missing recording subcommand. Use 'read' or 'set'." << std::endl;
            return false;
        }

        std::string subcommand = args[1];
        if (subcommand == "read") {
            return read_recording_status();
        } else if (subcommand == "set") {
            if (args.size() < 3) {
                std::cerr << "Error: Missing value for 'record set'" << std::endl;
                return false;
            }
            return set_recording_status(args[2]);
        }
        std::cerr << "Error: Unknown recording subcommand '" << subcommand << "'" << std::endl;
        return false;
    }

    std::string get_help() const override {
        return "  record read   Read recording permission status\n"
               "  record set <value>  Set recording permission (0/1, on/off, true/false, yes/no)";
    }

private:
    bool read_recording_status() {
        std::string value;
        if (!file_ops::read_file(ALLOW_RECORDING_PATH, value)) return false;
        std::cout << "Recording permission: " << (value == "1" ? "Enabled" : "Disabled") << std::endl;
        return true;
    }

    bool set_recording_status(const std::string& value) {
        std::string normalized_value;
        // Convert various input formats to "0" or "1"
        if (value == "0" || value == "off" || value == "false" || value == "no") {
            normalized_value = "0";
        } else if (value == "1" || value == "on" || value == "true" || value == "yes") {
            normalized_value = "1";
        } else {
            std::cerr << "Error: Value must be one of: 0/1, on/off, true/false, yes/no" << std::endl;
            return false;
        }
        
        if (!file_ops::write_file(ALLOW_RECORDING_PATH, normalized_value)) return false;
        std::cout << "Set recording permission to " << (normalized_value == "1" ? "Enabled" : "Disabled") << std::endl;
        return true;
    }
};

class KeyboardCommand : public Command {
public:
    bool execute(const std::vector<std::string>& args) override {
        if (args.size() < 2) {
            std::cerr << "Error: Missing keyboard subcommand. Use 'read' or 'set'." << std::endl;
            return false;
        }

        std::string subcommand = args[1];
        if (subcommand == "read") {
            return read_keyboard_backlight();
        } else if (subcommand == "set") {
            if (args.size() < 3) {
                std::cerr << "Error: Missing value for 'kbd set'" << std::endl;
                return false;
            }
            return set_keyboard_backlight(args[2]);
        }
        std::cerr << "Error: Unknown keyboard subcommand '" << subcommand << "'" << std::endl;
        return false;
    }

    std::string get_help() const override {
        return "  kbd read      Read keyboard backlight level\n"
               "  kbd set <0-3> Set keyboard backlight level (0=off, 1-3=brightness)\n"
               "               Note: Backlight may be affected by ambient light sensor\n"
               "               and GNOME's automatic backlight control";
    }

private:
    bool read_keyboard_backlight() {
        std::string value;
        if (!file_ops::read_file(KBD_BACKLIGHT_PATH, value)) return false;
        std::cout << "Keyboard backlight level: " << value << std::endl;
        return true;
    }

    bool set_keyboard_backlight(const std::string& value) {
        int val;
        try {
            val = std::stoi(value);
            if (val < 0 || val > 3) {
                std::cerr << "Error: Value must be between 0 and 3" << std::endl;
                return false;
            }
        } catch (...) {
            std::cerr << "Error: Invalid value '" << value << "'" << std::endl;
            return false;
        }
        if (!file_ops::write_file(KBD_BACKLIGHT_PATH, std::to_string(val))) return false;
        std::cout << "Set keyboard backlight level to " << val << std::endl;
        return true;
    }
};

class StartOnLidOpenCommand : public Command {
public:
    bool execute(const std::vector<std::string>& args) override {
        if (args.size() < 2) {
            std::cerr << "Error: Missing start-on-lid-open subcommand. Use 'read' or 'set'." << std::endl;
            return false;
        }

        std::string subcommand = args[1];
        if (subcommand == "read") {
            return read_status();
        } else if (subcommand == "set") {
            if (args.size() < 3) {
                std::cerr << "Error: Missing value for 'start-on-lid-open set'" << std::endl;
                return false;
            }
            return set_status(args[2]);
        }
        std::cerr << "Error: Unknown start-on-lid-open subcommand '" << subcommand << "'" << std::endl;
        return false;
    }

    std::string get_help() const override {
        return "  start-on-lid-open read   Read start on lid open status\n"
               "  start-on-lid-open set <value>  Set start on lid open (0/1, on/off, true/false, yes/no)";
    }

private:
    bool read_status() {
        std::string value;
        if (!file_ops::read_file(START_ON_LID_OPEN_PATH, value)) return false;
        std::cout << "Start on lid open: " << (value == "1" ? "Enabled" : "Disabled") << std::endl;
        return true;
    }

    bool set_status(const std::string& value) {
        std::string normalized_value;
        // Convert various input formats to "0" or "1"
        if (value == "0" || value == "off" || value == "false" || value == "no") {
            normalized_value = "0";
        } else if (value == "1" || value == "on" || value == "true" || value == "yes") {
            normalized_value = "1";
        } else {
            std::cerr << "Error: Value must be one of: 0/1, on/off, true/false, yes/no" << std::endl;
            return false;
        }
        
        if (!file_ops::write_file(START_ON_LID_OPEN_PATH, normalized_value)) return false;
        std::cout << "Set start on lid open to " << (normalized_value == "1" ? "Enabled" : "Disabled") << std::endl;
        return true;
    }
};

class UsbChargeCommand : public Command {
public:
    bool execute(const std::vector<std::string>& args) override {
        if (args.size() < 2) {
            std::cerr << "Error: Missing usb-charge subcommand. Use 'read' or 'set'." << std::endl;
            return false;
        }

        std::string subcommand = args[1];
        if (subcommand == "read") {
            return read_status();
        } else if (subcommand == "set") {
            if (args.size() < 3) {
                std::cerr << "Error: Missing value for 'usb-charge set'" << std::endl;
                return false;
            }
            return set_status(args[2]);
        }
        std::cerr << "Error: Unknown usb-charge subcommand '" << subcommand << "'" << std::endl;
        return false;
    }

    std::string get_help() const override {
        return "  usb-charge read   Read USB charge status\n"
               "  usb-charge set <value>  Set USB charge (0/1, on/off, true/false, yes/no)";
    }

private:
    bool read_status() {
        std::string value;
        if (!file_ops::read_file(USB_CHARGE_PATH, value)) return false;
        std::cout << "USB charge: " << (value == "1" ? "Enabled" : "Disabled") << std::endl;
        return true;
    }

    bool set_status(const std::string& value) {
        std::string normalized_value;
        // Convert various input formats to "0" or "1"
        if (value == "0" || value == "off" || value == "false" || value == "no") {
            normalized_value = "0";
        } else if (value == "1" || value == "on" || value == "true" || value == "yes") {
            normalized_value = "1";
        } else {
            std::cerr << "Error: Value must be one of: 0/1, on/off, true/false, yes/no" << std::endl;
            return false;
        }
        
        if (!file_ops::write_file(USB_CHARGE_PATH, normalized_value)) return false;
        std::cout << "Set USB charge to " << (normalized_value == "1" ? "Enabled" : "Disabled") << std::endl;
        return true;
    }
};

class HelpCommand : public Command {
public:
    explicit HelpCommand(const std::map<std::string, std::unique_ptr<Command>>& cmds) 
        : commands(cmds) {}

    bool execute(const std::vector<std::string>&) override {
        print_help();
        return true;
    }

    std::string get_help() const override {
        return "  help          Show this help message";
    }

private:
    void print_help() {
        std::cout << "Usage: samsung-cli <command> [<args>]\n"
                  << "CLI tool to control Samsung Galaxy Book features.\n\n"
                  << "Commands:\n";
        
        // Get help text from all commands
        for (const auto& [name, cmd] : commands) {
            std::cout << cmd->get_help() << "\n";
        }
    }

    const std::map<std::string, std::unique_ptr<Command>>& commands;
};

int main(int argc, char* argv[]) {
    std::map<std::string, std::unique_ptr<Command>> commands;
    
    // Create all other commands first
    commands["power"] = std::make_unique<PowerCommand>();
    commands["fan"] = std::make_unique<FanCommand>();
    commands["perf"] = std::make_unique<PerformanceCommand>();
    commands["record"] = std::make_unique<RecordingCommand>();
    commands["kbd"] = std::make_unique<KeyboardCommand>();
    commands["start-on-lid-open"] = std::make_unique<StartOnLidOpenCommand>();
    commands["usb-charge"] = std::make_unique<UsbChargeCommand>();
    
    // Create help command last since it needs reference to all commands
    commands["help"] = std::make_unique<HelpCommand>(commands);

    if (argc < 2) {
        commands["help"]->execute({});
        return 1;
    }

    std::string command = argv[1];
    std::vector<std::string> args(argv + 1, argv + argc);  // Skip program name

    auto it = commands.find(command);
    if (it == commands.end()) {
        std::cerr << "Error: Unknown command '" << command << "'" << std::endl;
        commands["help"]->execute({});
        return 1;
    }

    return it->second->execute(args) ? 0 : 1;
}
