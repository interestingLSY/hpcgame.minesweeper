.SECONDEXPANSION:

CC 	= g++
CXXFLAGS ?= -g -Ofast -std=c++17 -lpthread -lrt -Wall -march=native

LIBS = csapp wrappers minesweeper_helpers log common shm futex queue
EXES = judger game_server map_generator map_visualizer blank_counter naive naive_optim 

LIB_OBJS = $(foreach x, $(LIBS), $(addsuffix .o, $(x)))

.PHONY: all clean handin

all: $(EXES)

%.o: %.cpp	# for executables
	$(CC) -c $(CXXFLAGS) $< -o $@

%.o: lib/%.cpp lib/%.h	# for lib/?.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@

$(EXES): $$@.o $(LIB_OBJS)
	$(CC) $@.o $(CXXFLAGS) $(LIB_OBJS) -o $@

handin:
	# create the .zip file
	rm -f answer.zip
	zip answer.zip answer.cpp

clean:
	rm -rf *.o $(EXES)