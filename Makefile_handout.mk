.SECONDEXPANSION:

CC 	= g++
CXXFLAGS ?= -g -Ofast -std=c++17 -Wall -march=native -Wl,--as-needed -lpthread -lrt -pthread

LIBS = csapp wrappers minesweeper_helpers log common shm futex queue
EXES = judger game_server map_generator map_visualizer naive naive_optim interact answer

LIB_OBJS = $(foreach x, $(LIBS), $(addsuffix .o, $(x)))

.PHONY: all clean handin

all: $(EXES)

%.o: %.cpp	# for executables
	$(CC) -c $(CXXFLAGS) $< -o $@

%.o: lib/%.cpp lib/%.h	# for lib/?.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@

$(EXES): $$@.o $(LIB_OBJS)
	$(CC) $@.o $(LIB_OBJS) $(CXXFLAGS) -o $@

handin:
	# create the .zip file
	rm -f answer.zip
	zip answer.zip answer.cpp

clean:
	rm -rf *.o $(EXES)