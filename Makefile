CC=g++

CFLAGS= -c -Wall -std=c++11

all: ODIS

ODIS: main.o globals.o mesh.o field.o mass.o energy.o solver.o
	$(CC) main.o globals.o mesh.o field.o mass.o energy.o solver.o -o ODIS

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

globals.o: globals.cpp
	$(CC) $(CFLAGS) globals.cpp

mesh.o: mesh.cpp
	$(CC) $(CFLAGS) mesh.cpp

field.o: field.cpp
	$(CC) $(CFLAGS) field.cpp

mass.o: mass.cpp
	$(CC) $(CFLAGS) mass.cpp

energy.o: energy.cpp
	$(CC) $(CFLAGS) energy.cpp

solver.o: solver.cpp
	$(CC) $(CFLAGS) solver.cpp

clean:
	rm *o ODIS
