.SECONDEXPANSION:

CC 	= g++
CXXFLAGS ?= -g -Ofast -std=c++17 -lpthread -lrt -Wall -march=native	# `-lrt` for `shm_open()`

ANSWERS = naive naive-mt naive-optim interact simple-expand-single-thread
LIBS = csapp wrappers minesweeper_helpers log common shm futex queue
EXES = judger game_server map_generator map_visualizer blank_counter

# files to be put into the `handout` directory
HANDOUT_FILE_LIST = game_server.cpp judger.cpp map_generator.cpp map_visualizer.cpp \
	answer/naive.cpp answer/naive-optim.cpp generate_example_maps.py

LIB_OBJS = $(foreach x, $(LIBS), $(addsuffix .o, $(x)))

.PHONY: all handout clean

all: $(EXES) $(ANSWERS)

%.o: %.cpp	# for executables
	$(CC) -c $(CXXFLAGS) $< -o $@

%.o: answer/%.cpp	# for answer/?.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@

%.o: lib/%.cpp lib/%.h	# for lib/?.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@

$(EXES): $$@.o $(LIB_OBJS)
	$(CC) $@.o $(CXXFLAGS) $(LIB_OBJS) -o $@

$(ANSWERS): $$@.o $(LIB_OBJS)
	$(CC) $@.o $(CXXFLAGS) $(LIB_OBJS) -o $@

handout: all
	rm -rf minesweeper_handout/*
	# copy executables and answers
	$(foreach file, $(HANDOUT_FILE_LIST), cp $(file) minesweeper_handout/.;)
	# copy libraries
	cp -r lib minesweeper_handout/lib
	cp lib/minesweeper_helpers.h minesweeper_handout/.
	# copy makefile
	cp Makefile_handout.mk minesweeper_handout/Makefile

clean:
	rm -rf *.o $(EXES) $(ANSWERS)