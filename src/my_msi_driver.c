#include <hidapi/hidapi.h>
#include <sensors/sensors.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define TRUE 1
#define FALSE 0

typedef enum FanMode { SILENT = 0, BALANCE = 1, GAME = 2, CUSTOMIZE = 3, DEFAULT = 4, SMART = 5 } FanMode;
typedef enum Option { OPT_UNKNOWN, OPT_MODE, OPT_STARTD } Option;

int should_stop = FALSE;

Option option_by_string(const char *arg);
void set_fan_mode(hid_device *handle, int fan_mode);
const int get_temperature_chip_subfeature(const sensors_subfeature *subfeature);
int run_monitor_daemon(hid_device *handle, const sensors_subfeature *subfeature, int *stop_flag);

void stop_handler() { should_stop = TRUE; }

int main(int argc, char *argv[]) {
    int fan_mode = SMART;
    int should_start_daemon = FALSE;

    for (int i = 1; i < argc; i++) {
        switch (option_by_string(argv[i])) {
            case OPT_MODE:
                // TODO: rewrite with strtol
                switch (atoi(argv[MIN(i + 1, argc - 1)])) {
                    case 0:
                        fan_mode = 0;
                        break;
                    case 2:
                        fan_mode = 2;
                    case 1:
                        fan_mode = 1;
                        break;
                    case 4:
                        fan_mode = 4;
                        break;
                    case 5:
                        fan_mode = 5;
                        break;
                    default:
                        printf("Allowed modes:\n");
                        printf("0 : silent\n");
                        printf("1 : balance\n");
                        printf("2 : game\n");
                        printf("4 : default (constant)\n");
                        printf("5 : smart\n");
                        exit(1);
                        break;
                }
                break;
            case OPT_STARTD:
                should_start_daemon = TRUE;
                break;
            default:
                printf("Usage: -M [0-5] startd\n");
                exit(1);
                break;
        }
    }

    if (!hid_init()) {
        fprintf(stderr, "%ls", hid_error(NULL));
        hid_exit();
        exit(1);
    }

    hid_device *hid_device = NULL;
    if (!(hid_device = hid_open(0x0db0, 0x6a05, NULL))) {
        fprintf(stderr, "%ls", hid_error(NULL));
        hid_exit();
        exit(1);
    }

    set_fan_mode(hid_device, fan_mode);

    if (should_start_daemon) {
        signal(SIGTERM, stop_handler);

        const sensors_subfeature *temperature_input_subfeature;
        if (!get_temperature_chip_subfeature(temperature_input_subfeature)) {
            fprintf(stderr, "Unable to get subfeature\n");
            hid_close(hid_device);
            hid_exit();
            exit(1);
        }

        if (!run_monitor_daemon(hid_device, temperature_input_subfeature, &should_stop)) {
            fprintf(stderr, "Daemon exited abnormally.\n");
            hid_close(hid_device);
            hid_exit();
            exit(1);
        }
    }
    hid_close(hid_device);
    hid_exit();

    exit(0);
}

const int get_temperature_chip_subfeature(const sensors_subfeature *chip_subfeature) {
    const sensors_chip_name *chip_name;
    const sensors_feature *chip_feature;

    for (int chip_index = 0; (chip_name = sensors_get_detected_chips(NULL, &chip_index)) != NULL;)
        if (!strcmp(chip_name->prefix, "coretemp") || !strcmp(chip_name->prefix, "k10temp"))
            for (int feature_index = 0;
                 (chip_feature = sensors_get_features(chip_name, &feature_index)) != NULL;)
                if (chip_feature->type == SENSORS_FEATURE_TEMP &&
                    (!strcmp(chip_feature->name, "temp1") || !strcmp(chip_feature->name, "Tctl")))
                    for (int subfeature_index = 0; (chip_subfeature = sensors_get_all_subfeatures(
                                                        chip_name, chip_feature, &subfeature_index)) != NULL;)
                        if (chip_subfeature->type == SENSORS_SUBFEATURE_TEMP_INPUT) return 0;

    return -1;
}

int get_chip_subfeature_temperature(const sensors_subfeature *subfeature, double *temperature) {
    return sensors_get_value(NULL, subfeature->number, temperature);
}

int write_chip_subfeature_temperature(hid_device *hid_device, double temperature) {
    int temperature_int;
    unsigned char chip_settings[65];

    temperature_int = (int)temperature;
    memset(chip_settings, 0, sizeof(chip_settings));
    chip_settings[0] = 0xD0;
    chip_settings[1] = 0x85;
    chip_settings[4] = temperature_int & 0xFF;
    chip_settings[5] = (temperature_int >> 8) & 0xFF;

    return hid_write(hid_device, chip_settings, 65);
}

int run_monitor_daemon(hid_device *hid_device, const sensors_subfeature *subfeature, int *stop_flag) {
    double temperature;
    while (!(*stop_flag)) {
        if (!get_chip_subfeature_temperature(subfeature, &temperature)) {
            fprintf(stderr, "Could not read chip subfeature temperature\n");
            return -1;
        }
        if (!write_chip_subfeature_temperature(hid_device, temperature)) {
            fprintf(stderr, "Could not write chip subfeature temperature\n");
            return -1;
        }
        usleep(2000 * 1000);  // 2 seconds
    }
    return 0;
}

/**
 * Set the fan mode.
 *
 * \param handle handle on the AIO device
 * \param fan_mode the choosen fan mode
 */
void set_fan_mode(hid_device *hid_device, int fan_mode) {
    unsigned char chip_settings[65];

    memset(chip_settings, 0, sizeof(chip_settings));
    chip_settings[0] = 0xD0;
    chip_settings[1] = 0x40;
    chip_settings[2] = fan_mode;
    chip_settings[10] = fan_mode;
    chip_settings[18] = fan_mode;
    chip_settings[26] = fan_mode;
    chip_settings[34] = fan_mode;
    hid_write(hid_device, chip_settings, 65);
    chip_settings[1] = 0x41;
    hid_write(hid_device, chip_settings, 65);
}

Option option_by_string(const char *arg) {
    if (strcmp(arg, "-M") == 0) return OPT_MODE;
    if (strcmp(arg, "startd") == 0) return OPT_STARTD;
    return OPT_UNKNOWN;
}
