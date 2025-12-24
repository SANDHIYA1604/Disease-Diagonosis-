CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99
TARGET = disease_server.exe
OBJS = server.o disease_engine.o csv_parser.o
LIBS = -lws2_32

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

server.o: server.c disease_engine.h
	$(CC) $(CFLAGS) -c server.c

disease_engine.o: disease_engine.c disease_engine.h csv_parser.h
	$(CC) $(CFLAGS) -c disease_engine.c

csv_parser.o: csv_parser.c csv_parser.h disease_engine.h
	$(CC) $(CFLAGS) -c csv_parser.c

clean:
	del /Q $(OBJS) $(TARGET)

run: all
	$(TARGET)

.PHONY: all clean run