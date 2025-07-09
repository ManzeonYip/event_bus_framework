CC = gcc
CFLAGS = -Wall -pthread
SRCDIR = ./src
SRC = $(SRCDIR)/event_bus.c $(SRCDIR)/example_usage.c
OBJ = $(SRC:.c=.o)
TARGET = event_bus_framework

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: $(SRCDIR)/%.c $(SRCDIR)/event_bus.h
	$(CC) $(CFLAGS) -c $< -o $(SRCDIR)/$@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: clean
