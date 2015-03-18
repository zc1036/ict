
CC := g++
FLAGS := -Wall -std=c++11

HEADERS := 
OBJECTS := main.o

LIBRARIES := -lecl

debug: FLAGS += -O0 -g

bin/ictest-dbg: $(OBJECTS)
	$(CC) $(FLAGS) -o $@ $(OBJECTS) $(LIBRARIES)

$(OBJECTS): %.o: src/%.c++ $(HEADERS)
	$(CC) $(FLAGS) -c -o $@ $<
