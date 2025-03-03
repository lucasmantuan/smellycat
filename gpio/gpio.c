#include "gpio.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define GPIO_DIR "/sys/class/gpio"

int gpio_export(int gpio) {
    char buffer[64];
    int fd = open(GPIO_DIR "/export", O_WRONLY);
    if (fd == -1) {
        perror("erro ao exportar");
        return -1;
    }
    sprintf(buffer, "%d", gpio);
    write(fd, buffer, strlen(buffer));
    close(fd);
    return 0;
}

int gpio_unexport(int gpio) {
    char buffer[64];
    int fd = open(GPIO_DIR "/unexport", O_WRONLY);
    if (fd == -1) {
        perror("erro ao remover");
        return -1;
    }
    sprintf(buffer, "%d", gpio);
    write(fd, buffer, strlen(buffer));
    close(fd);
    return 0;
}

int gpio_set_direction(int gpio, const char *direction) {
    char path[64];
    sprintf(path, GPIO_DIR "/gpio%d/direction", gpio);
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("erro ao definir direção");
        return -1;
    }
    write(fd, direction, strlen(direction));
    close(fd);
    return 0;
}

int gpio_write(int gpio, int value) {
    char path[64];
    sprintf(path, GPIO_DIR "/gpio%d/value", gpio);
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("erro ao escrever");
        return -1;
    }
    dprintf(fd, "%d", value);
    close(fd);
    return 0;
}

int gpio_read(int gpio) {
    char path[64];
    char value_str[3];
    sprintf(path, GPIO_DIR "/gpio%d/value", gpio);
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("erro ao ler");
        return -1;
    }
    read(fd, value_str, 3);
    close(fd);
    return atoi(value_str);
}