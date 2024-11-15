CC = gcc
CFLAGS = -Wall

TARGET = smellycat
SRC = smellycat.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)