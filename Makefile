.SECONDEXPANSION:

CC 	= g++
CXXFLAGS ?= -g -Ofast -std=c++17 -Wall -march=native -Wl,--as-needed -pthread -lpthread -lrt	# `-lrt` for `shm_open()`

ANSWERS = template naive naive_mt naive_optim just_open_many_channels interact simple_expand_single_thread expand_with_queue expand_with_queue_mt
LIBS = csapp wrappers minesweeper_helpers log common shm futex queue
EXES = judger game_server map_generator map_visualizer blank_counter

# files to be put into the `handout` directory
HANDOUT_FILE_LIST = game_server.cpp judger.cpp map_generator.cpp map_visualizer.cpp \
	answer/naive.cpp answer/naive_optim.cpp answer/interact.cpp generate_example_maps.py

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
	$(CC) $@.o $(LIB_OBJS) $(CXXFLAGS) -o $@

$(ANSWERS): $$@.o $(LIB_OBJS)
	$(CC) $@.o $(LIB_OBJS) $(CXXFLAGS) -o $@

handout: all
	rm -rf minesweeper_handout
	mkdir minesweeper_handout
	# copy executables and answers
	$(foreach file, $(HANDOUT_FILE_LIST), cp $(file) minesweeper_handout/.;)
	# copy libraries
	cp -r lib minesweeper_handout/lib
	cp lib/minesweeper_helpers.h minesweeper_handout/.
	# copy makefile
	cp Makefile_handout.mk minesweeper_handout/Makefile
	# copy answer template
	cp answer/template.cpp minesweeper_handout/answer.cpp
	# create the zipball
	rm -rf minesweeper_handout.zip
	zip -r -q minesweeper_handout.zip minesweeper_handout/

clean:
	rm -rf *.o $(EXES) $(ANSWERS)
