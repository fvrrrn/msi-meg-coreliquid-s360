#include <hidapi/hidapi.h>
#include <sensors/sensors.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum FanMode { SILENT = 0, BALANCE = 1, GAME = 2, CUSTOMIZE = 3, DEFAULT = 4, SMART = 5 } FanMode;

int stop = 0;

void set_fan_mode(hid_device *handle, int fan_mode);
const int get_temperature_chip_subfeature(const sensors_subfeature *subfeature);
int run_monitor_daemon(hid_device *handle, const sensors_subfeature *subfeature, int *stop_flag);

void stop_handler() { stop = 1; }

int main(int argc, char *argv[]) {
    int i, fan_mode = SMART;
    int start_daemon = 0;
    hid_device *handle = NULL;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-M")) {
            fan_mode = atoi(argv[++i]);
            if ((fan_mode < 0) || (fan_mode > 5) || (fan_mode == 3)) {
                printf("Allowed modes:\n");
                printf("0 : silent\n");
                printf("1 : balance\n"), printf("2 : game\n");
                printf("4 : default (constant)\n");
                printf("5 : smart\n");
                exit(0);
            }
        } else if (!strcmp(argv[i], "startd"))
            start_daemon = 1;
    }

    int res = hid_init();
    printf("hid_init status: %d\n", res);
    // Open the device using the VID, PID
    handle = hid_open(0x0db0, 0x6a05, NULL);
    if (!handle) {
        fprintf(stderr, "Unable to open device\n");
        hid_exit();
        exit(1);
    }

    set_fan_mode(handle, fan_mode);
    if (start_daemon) {
        signal(SIGTERM, stop_handler);

        const sensors_subfeature *subfeature;
        if (!get_temperature_chip_subfeature(subfeature)) {
            fprintf(stderr, "Unable to get subfeature\n");
            hid_close(handle);
            hid_exit();
            exit(1);
        }

        if (!run_monitor_daemon(handle, subfeature, &stop)) {
            fprintf(stderr, "Daemon exited abnormally.\n");
            hid_close(handle);
            hid_exit();
            exit(1);
        }
    }
    hid_close(handle);
    hid_exit();

    exit(0);
}

const int get_temperature_chip_subfeature(const sensors_subfeature *subfeature) {
    const sensors_chip_name *chip;
    const sensors_feature *feature;

    for (int nr = 0; (chip = sensors_get_detected_chips(NULL, &nr)) != NULL;)
        if (!strcmp(chip->prefix, "coretemp") || !strcmp(chip->prefix, "k10temp"))
            for (int nf = 0; (feature = sensors_get_features(chip, &nf)) != NULL;)
                if (feature->type == SENSORS_FEATURE_TEMP &&
                    (!strcmp(feature->name, "temp1") || !strcmp(feature->name, "Tctl")))
                    for (int ns = 0; (subfeature = sensors_get_all_subfeatures(chip, feature, &ns)) != NULL;)
                        if (subfeature->type == SENSORS_SUBFEATURE_TEMP_INPUT) return 0;

    return -1;
}

int get_chip_subfeature_temperature(const sensors_subfeature *subfeature, double *temperature) {
    return sensors_get_value(NULL, subfeature->number, temperature);
}

int write_chip_subfeature_temperature(hid_device *handle, double temperature) {
    int itemp;
    unsigned char buf[65];

    itemp = (int)temperature;
    memset(buf, 0, sizeof(buf));
    buf[0] = 0xD0;
    buf[1] = 0x85;
    buf[4] = itemp & 0xFF;
    buf[5] = (itemp >> 8) & 0xFF;

    return hid_write(handle, buf, 65);
}

int run_monitor_daemon(hid_device *handle, const sensors_subfeature *subfeature, int *stop_flag) {
    double temp;
    while (!(*stop_flag)) {
        if (!get_chip_subfeature_temperature(subfeature, &temp)) {
            fprintf(stderr, "error\n");
            return 1;
        }
        if (!write_chip_subfeature_temperature(handle, temp)) {
            fprintf(stderr, "error\n");
            return 1;
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
void set_fan_mode(hid_device *handle, int fan_mode) {
    unsigned char buf[65];

    memset(buf, 0, sizeof(buf));
    buf[0] = 0xD0;
    buf[1] = 0x40;
    buf[2] = fan_mode;
    buf[10] = fan_mode;
    buf[18] = fan_mode;
    buf[26] = fan_mode;
    buf[34] = fan_mode;
    hid_write(handle, buf, 65);
    buf[1] = 0x41;
    hid_write(handle, buf, 65);
}
