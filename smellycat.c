#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include "gpio/gpio.h"

#define ECHO 269
#define TRIG 268
#define MEASURAMENTS 5
#define GAP 1

double measure_distance() {
    struct timeval start, end;
    double distance;

    gpio_write(TRIG, 1);
    usleep(10);
    gpio_write(TRIG, 0);

    // Tempo Inicial
    while (gpio_read(ECHO) == 0);
    gettimeofday(&start, NULL);
    // printf("ECHO: %d\n", ECHO);
    
    // Tempo Final
    while (gpio_read(ECHO) == 1);
    gettimeofday(&end, NULL);

    // Calcula o Tempo em Microssegundos
    double elapsed_time = (end.tv_sec - start.tv_sec) * 1000000.0 + (end.tv_usec - start.tv_usec);
    
    // Calcula a Distância com Base na Velocidade do Som
    distance = (elapsed_time * 0.0343) / 2.0;

    return distance;
}

double smooth_distance() {
    double sum = 0;
    for (int i = 0; i < MEASURAMENTS; i++) {
        sum += measure_distance();
        usleep(100000);
    }
    return sum / MEASURAMENTS;
}


int main()
{
    gpio_export(TRIG);
    gpio_export(ECHO);
    
    usleep(500000);

    gpio_set_direction(TRIG, "out");
    gpio_set_direction(ECHO, "in");

    usleep(500000);
    
    double prev_distance = smooth_distance();
    printf("Primeira Medição: %.2f cm\n", prev_distance);
    
    while (1)
    {
        double current_distance = smooth_distance();
        
        if (current_distance <= (prev_distance - GAP))
        {
            printf("Distância: %.2f cm\n", current_distance);
        }

        usleep(1000000);
    }

    return 0;
}
