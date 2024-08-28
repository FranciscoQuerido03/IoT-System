CC = gcc
CFLAGS = -Wall -Werror

all: home_iot console sensor

home_iot: SystemManager.o
	$(CC) $(CFLAGS) -o home_iot SystemManager.o

console: User_console.o
	$(CC) $(CFLAGS) -o console User_console.o

sensor: Sensor.o
	$(CC) $(CFLAGS) -o sensor Sensor.o

SystemManager.o: SystemManager.c
	$(CC) $(CFLAGS) -c SystemManager.c

User_console.o: User_console.c
	$(CC) $(CFLAGS) -c User_console.c

Sensor.o: Sensor.c
	$(CC) $(CFLAGS) -c Sensor.c

clean:
	rm -f home_iot console sensor *.o

