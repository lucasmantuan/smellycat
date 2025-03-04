#ifndef MQ_H
#define MQ_H

#include <stdint.h>

#define CONFIG_REG 0x01
#define CONVERSION_REG 0x00

int configure_device(int fd, uint16_t config);
int16_t read_conversion_register(int fd);
float calibrate_sensor(int fd, uint16_t channel, float vcc, float RL);
float read_calibration(const char *filename);
int save_calibration(const char *filename, float R0);

#endif
