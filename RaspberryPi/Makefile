CC=gcc
CXX=g++
LD=g++
CFLAGS=-c -Wall -pedantic-errors -O3 -std=gnu11 -D_GNU_SOURCE -I./
LDFLAGS=
OBJECTS=simplehttp.o ./exp1lib.o
EXECUTABLE=simplehttp

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CXX) $(CFLAGS) $< -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm -f ${EXECUTABLE} ${EXECUTABLE}.exe ${OBJECTS} core

