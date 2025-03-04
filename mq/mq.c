#include "mq.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

int configure_device(int fd, uint16_t config)
{
    uint8_t buffer[3];
    buffer[0] = CONFIG_REG;
    buffer[1] = (config >> 8) & 0xFF;
    buffer[2] = config & 0xFF;
    if (write(fd, buffer, 3) != 3)
    {
        perror("configure device error");
        return -1;
    }
    usleep(100000);
    return 0;
}

int16_t read_conversion_register(int fd)
{
    uint8_t buffer[2];
    buffer[0] = CONVERSION_REG;
    if (write(fd, buffer, 1) != 1)
    {
        perror("read conversion register error");
        return 0xFFFF;
    }
    if (read(fd, buffer, 2) != 2)
    {
        perror("read conversion register error");
        return 0xFFFF;
    }
    int16_t result = (buffer[0] << 8) | buffer[1];
    return result;
}

float calibrate_sensor(int fd, uint16_t channel, float vcc, float RL)
{
    if (configure_device(fd, channel) != 0)
    {
        return -1.0;
    }
    usleep(100000);
    int num_samples = 1000;
    float sum_RS = 0.0;
    for (int i = 0; i < num_samples; i++)
    {
        int16_t raw_value = read_conversion_register(fd);
        if (raw_value == 0xFFFF)
        {
            return -1.0;
        }
        float vout = raw_value * vcc / 32768.0;
        float RS = ((vcc - vout) * RL) / vout;
        sum_RS += RS;
        usleep(100000);
    }
    return sum_RS / num_samples;
}

float read_calibration(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        return -1.0;
    }
    float R0 = -1.0;
    if (fscanf(file, "%f", &R0) != 1)
    {
        fclose(file);
        return -1.0;
    }
    fclose(file);
    return R0;
}

int save_calibration(const char *filename, float R0)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        return -1;
    }
    fprintf(file, "%.6f\n", R0);
    fclose(file);
    return 0;
}
