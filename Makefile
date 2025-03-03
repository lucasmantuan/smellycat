CC = gcc
CFLAGS = -Wall -Igpio
LDFLAGS = 

TARGET = smellycat
SRC = smellycat.c gpio/gpio.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
