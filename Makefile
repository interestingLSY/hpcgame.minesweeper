.SECONDEXPANSION:

CC 	= g++
CXXFLAGS ?= -g -Ofast -std=c++20 -lpthread -Wall -march=native

ANSWERS = naive naive-mt
LIBS = csapp wrappers minesweeper_helpers
EXES = judger game_server map_generator map_visualizer

LIB_OBJS = $(foreach x, $(LIBS), $(addsuffix .o, $(x)))

.PHONY: all handout clean

all: $(EXES) $(ANSWERS)

%.o: %.cpp	# for judger.cpp and game_server.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@

%.o: answer/%.cpp	# for answer/?.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@

%.o: lib/%.cpp lib/%.h	# for lib/?.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@

$(EXES): $$@.o $(LIB_OBJS)
	$(CC) $@.o $(CXXFLAGS) $(LIB_OBJS) -o $@

$(ANSWERS): $$@.o $(LIB_OBJS)
	$(CC) $@.o $(CXXFLAGS) $(LIB_OBJS) -o $@

grade: all

handout: all

clean:
	rm -rf *.o $(EXES) $(ANSWERS)