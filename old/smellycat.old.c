#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

#define DEVICE "/dev/i2c-2"
#define ADDRESS_DEVICE 0x48

// Registrador de Configuração
#define CONFIG_REG 0x01

// Registrador de Conversão
#define CONVERSION_REG 0x00

// Configuração do Canal A0 e A1 (Modo Single-Shot)
#define CHANNEL_A0 0xC383
#define CHANNEL_A1 0xD383

// Configuração do Ganho A0 e A1
#define GAIN_A0 4.096
#define GAIN_A1 4.096

// Arquivos de Calibração dos Sensores
#define CALIBRATION_FILE_MQ135 "/root/smellycat/mq135.txt"
#define CALIBRATION_FILE_MQ2 "/root/smellycat/mq2.txt"

// Tensão de Alimentação dos Sensores em Volts
#define VCC 5.0

// Resistência de Carga para o MQ135 e MQ2
#define RL_MQ135 20.0
#define RL_MQ2 5.0

#define HISTORY_SIZE 10

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

uint16_t read_config_register(int fd)
{
    uint8_t buffer[1];
    buffer[0] = CONFIG_REG;

    if (write(fd, buffer, 1) != 1)
    {
        perror("error reading from i2c");
        return 0xFFFF;
    }

    uint8_t data[2];
    if (read(fd, data, 2) != 2)
    {
        perror("error reading from i2c");
        return 0xFFFF;
    }

    uint16_t config_val = (data[0] << 8) | data[1];
    return config_val;
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

int16_t read_conversion_register_with_checking(int fd)
{
    while (1)
    {
        uint16_t config_val = read_config_register(fd);

        if (config_val == 0xFFFF)
        {
            return 0xFFFF;
        }

        if (config_val & 0x8000)
        {
            break;
        }

        usleep(10000);
    }

    return read_conversion_register(fd);
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
        int16_t raw_value = read_conversion_register_with_checking(fd);

        if (raw_value == 0xFFFF)
        {
            return -1.0;
        }

        float vout = raw_value * vcc / 32768.0;
        float RS = ((vcc - vout) * RL) / vout;
        sum_RS += RS;
        usleep(100000);
    }

    float R0 = sum_RS / num_samples;
    return R0;
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

int main()
{
    FILE *log = fopen("/root/smellycat/log.txt", "a");
    float ratio_a0_history[HISTORY_SIZE] = {0.0};
    float ratio_a1_history[HISTORY_SIZE] = {0.0};
    bool history_full = false;
    int history_index = 0;
    int fd;

    if (log == NULL)
    {
        perror("log error");
        return 1;
    }

    if ((fd = open(DEVICE, O_RDWR)) < 0)
    {
        perror("i2c error");
        fclose(log);
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, ADDRESS_DEVICE) < 0)
    {
        perror("device error");
        close(fd);
        fclose(log);
        return 1;
    }

    // Calibração do Sensor MQ135
    float R0_MQ135 = read_calibration(CALIBRATION_FILE_MQ135);

    if (R0_MQ135 < 0)
    {
        printf("calibrate sensor\n");
        R0_MQ135 = calibrate_sensor(fd, CHANNEL_A0, VCC, RL_MQ135);

        if (R0_MQ135 < 0)
        {
            perror("calibrate sensor error");
            close(fd);
            fclose(log);
            return 1;
        }

        if (save_calibration(CALIBRATION_FILE_MQ135, R0_MQ135) != 0)
        {
            perror("calibrate sensor error");
        }
    }

    // Calibração do Sensor MQ2
    float R0_MQ2 = read_calibration(CALIBRATION_FILE_MQ2);
    if (R0_MQ2 < 0)
    {
        printf("calibrate sensor\n");
        R0_MQ2 = calibrate_sensor(fd, CHANNEL_A1, VCC, RL_MQ2);

        if (R0_MQ2 < 0)
        {
            perror("calibrate sensor error");
            close(fd);
            fclose(log);
            return 1;
        }

        if (save_calibration(CALIBRATION_FILE_MQ2, R0_MQ2) != 0)
        {
            perror("calibrate sensor error");
        }
    }

    printf("successful calibration\n");

    while (1)
    {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%d/%m/%Y %H:%M:%S", t);

        // Configuração e leitura do MQ135
        if (configure_device(fd, CHANNEL_A0) != 0)
        {
            close(fd);
            fclose(log);
            return 1;
        }

        usleep(100000);

        int16_t raw_value_a0 = read_conversion_register_with_checking(fd);
        if (raw_value_a0 == 0xFFFF)
        {
            break;
        }

        float vout_a0 = raw_value_a0 * VCC / 32768.0;
        float RS_a0 = ((VCC - vout_a0) * RL_MQ135) / vout_a0;
        float ratio_a0 = R0_MQ135 / RS_a0;

        // Configuração e leitura do MQ2
        if (configure_device(fd, CHANNEL_A1) != 0)
        {
            close(fd);
            fclose(log);
            return 1;
        }

        usleep(100000);

        int16_t raw_value_a1 = read_conversion_register_with_checking(fd);
        if (raw_value_a1 == 0xFFFF)
        {
            break;
        }

        float vout_a1 = raw_value_a1 * VCC / 32768.0;
        float RS_a1 = ((VCC - vout_a1) * RL_MQ2) / vout_a1;
        float ratio_a1 = R0_MQ2 / RS_a1;

        ratio_a0_history[history_index] = ratio_a0;
        ratio_a1_history[history_index] = ratio_a1;

        history_index++;

        if (history_index >= HISTORY_SIZE)
        {
            history_index = 0;
            history_full = true;
        }

        float sum_a0 = 0.0;
        float sum_a1 = 0.0;
        int count = (history_full) ? HISTORY_SIZE : history_index;

        for (int i = 0; i < count; i++)
        {
            sum_a0 += ratio_a0_history[i];
            sum_a1 += ratio_a1_history[i];
        }

        float avg_a0 = sum_a0 / count;
        float avg_a1 = sum_a1 / count;

        printf("TIME: %s - MQ135: %.3f - MQ2: %.3f\n", time_str, avg_a0, avg_a1);
        fprintf(log, "%s; %.3f; %.3f;\n", time_str, avg_a0, avg_a1);

        fflush(log);
        sleep(30);
    }

    close(fd);
    fclose(log);
    return 0;
}
