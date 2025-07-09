CC = gcc
CFLAGS = -Wall -pthread
SRC = src/event_bus.c src/example_usage.c
OBJ = $(SRC:.c=.o)
TARGET = event_bus_app

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: src/%.c src/event_bus.h
	$(CC) $(CFLAGS) -c $< -o src/$@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: clean
