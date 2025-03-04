#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include "gpio/gpio.h"

// Define as Configurações Básicas do Dispositivo I2C e Aplicação
#define DEVICE "/dev/i2c-2"
#define ADDRESS_DEVICE 0x48
#define HISTORY_SIZE 10

// Registrador de Configuraçãoe de Conversão
#define CONFIG_REG 0x01
#define CONVERSION_REG 0x00

// Configuração do Canal A1 (Modo Continuo)
#define CHANNEL_A1 0xD083

// Configuração do Ganho e Resistência de Carga para o MQ2
#define GAIN_A1 6.144
#define RL_MQ2 5.0

// Arquivos de Calibração dos Sensores
#define CALIBRATION_FILE_MQ2 "/root/smellycat/data/mq2.txt"

// Tensão de Alimentação do Sensor em Volts
#define VCC 5.0

// Configura o Dispositivo I2C Para Operar no Canal, Modo de Operação e Outros Parâmetros Definidos
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

// Lê o Registrador de Configuração do Dispositivo I2C e Retorna seu Valor Atual
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

// Lê o Registrador de Conversão do Dispositivo I2C e Obtém o Valor Convertido Pelo ADC
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

// Realiza a Calibração Inicial do Sensor Calculando o Valor Médio de R0
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
    float R0 = sum_RS / num_samples;
    return R0;
}

// Lê o Valor de Calibração (R0) a Partir de um Arquivo de Texto
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

// Salva o Valor de Calibração (R0) em um Arquivo de Texto
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
    FILE *log = fopen("/root/smellycat/data/log.txt", "a");
    float ratio_a1_history[HISTORY_SIZE] = {0.0};
    bool history_full = false;
    int history_index = 0;
    int fd;

    // Verifica se Houve Falha na Abertura do Arquivo de Log
    if (log == NULL)
    {
        perror("log error");
        return 1;
    }

    // Executa um Teste de Leitura/Escrita no Dispositivo I2C
    if ((fd = open(DEVICE, O_RDWR)) < 0)
    {
        perror("i2c error");
        fclose(log);
        return 1;
    }

    // Configura o Dispositivo I2C Para se Comunicar com o Endereço Especificado
    if (ioctl(fd, I2C_SLAVE, ADDRESS_DEVICE) < 0)
    {
        perror("device error");
        close(fd);
        fclose(log);
        return 1;
    }

    // Configuração do Canal Antes da Calibração e do Loop
    if (configure_device(fd, CHANNEL_A1) != 0)
    {
        perror("device configuration error");
        close(fd);
        fclose(log);
        return 1;
    }

    // Verificação e Calibração do Sensor
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

        int16_t raw_value_a1 = read_conversion_register(fd);

        if (raw_value_a1 == 0xFFFF)
        {
            perror("sensor read error");
            break;
        }

        float vout_a1 = raw_value_a1 * VCC / 32768.0;
        float RS_a1 = ((VCC - vout_a1) * RL_MQ2) / vout_a1;
        float ratio_a1 = R0_MQ2 / RS_a1;

        ratio_a1_history[history_index] = ratio_a1;
        history_index++;

        if (history_index >= HISTORY_SIZE)
        {
            history_index = 0;
            history_full = true;
        }

        float sum_a1 = 0.0;
        int count = (history_full) ? HISTORY_SIZE : history_index;

        for (int i = 0; i < count; i++)
        {
            sum_a1 += ratio_a1_history[i];
        }

        float avg_a1 = sum_a1 / count;

        printf("TIME: %s - MQ2: %.3f\n", time_str, avg_a1);
        fprintf(log, "%s; %.3f;\n", time_str, avg_a1);
        fflush(log);

        sleep(30);
    }

    close(fd);
    fclose(log);
    return 0;
}
