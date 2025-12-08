#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ch376.h"

#define MAX_LINE_LENGTH 256

void parse_hex(const char *hex_str, uint8_t *buffer, int max_len) {
    char *ptr = strdup(hex_str);
    char *token = strtok(ptr, " ");
    int i = 0;
    while (token != NULL && i < max_len) {
        buffer[i++] = (uint8_t)strtol(token, NULL, 16);
        token = strtok(NULL, " ");
    }
    free(ptr);
}



void parse_usb_cfg(const char *filename, usb_device_descriptor_t dev_desc) {
    FILE *file = fopen(filename, "r");

    char cwd[PATH_MAX]; // PATH_MAX est défini dans <limits.h>


    if (file == NULL) {
        printf("[CH376 plugin] Erreur ouverture fichier %s\n", filename);
        exit(1);
        return;
    }

    char line[MAX_LINE_LENGTH];
    uint8_t config_desc[64] = {0};
    uint8_t report_desc[64] = {0};
    int config_desc_len = 0;
    int report_desc_len = 0;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == ';' || line[0] == '\n')
            continue;

        if (strstr(line, "[USB_DEVICE_DESCRIPTOR]")) {
            while (fgets(line, sizeof(line), file) && line[0] != '[') {
                if (sscanf(line, "bLength = %hhx", &dev_desc.bLength) == 1) continue;
                if (sscanf(line, "bDescriptorType = %hhx", &dev_desc.bDescriptorType) == 1) continue;
                if (sscanf(line, "bcdUSB = %hx", &dev_desc.bcdUSB) == 1) continue;
                if (sscanf(line, "bDeviceClass = %hhx", &dev_desc.bDeviceClass) == 1) continue;
                if (sscanf(line, "idVendor = %hx", &dev_desc.idVendor) == 1) continue;
                if (sscanf(line, "idProduct = %hx", &dev_desc.idProduct) == 1) continue;
                // ... (autres champs)
            }
        }
        else if (strstr(line, "[USB_HID_REPORT_DESCRIPTOR]")) {
            char *ptr = strstr(line, "Data = ");
            if (ptr) {
                parse_hex(ptr + 7, report_desc, sizeof(report_desc));
                report_desc_len = strlen(ptr + 7) / 3; // Approximation
            }
        }
    }

    fclose(file);

}
