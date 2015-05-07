CC=g++
CFLAGS=-c -g -Wall -D_DEBUG -rdynamic
LDFLAGS=-g -rdynamic

SOURCES= \
TinyJS.cpp \
TinyJS_Functions.cpp \
TinyJS_MathFunctions.cpp \
TinyJS_SyntaxTree.cpp


OBJECTS=$(SOURCES:.cpp=.o)

all: run_tests Script run_profiler

run_tests: run_tests.o $(OBJECTS)
	$(CC) $(LDFLAGS) run_tests.o $(OBJECTS) -o $@

Script: Script.o $(OBJECTS)
	$(CC) $(LDFLAGS) Script.o $(OBJECTS) -o $@

run_profiler: run_profiler.o $(OBJECTS)
	$(CC) $(LDFLAGS) run_profiler.o $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f run_tests Script run_tests.o run_profiler.o Script.o $(OBJECTS)
