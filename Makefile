
CC := g++
FLAGS := -Wall -std=c++1y -fPIC -pie -rdynamic -Iinclude

HEADERS := Makefile include/ict/ict.h++ include/ict/internal/dll.h++
OBJECTS := main.o
BIN := bin/ict-dbg

LIBRARIES := -ldl -lecl -lgc

debug: FLAGS += -O0 -g

$(BIN): $(OBJECTS)
	$(CC) $(FLAGS) -o $@ $(OBJECTS) $(LIBRARIES)

$(OBJECTS): %.o: src/%.c++ $(HEADERS)
	$(CC) $(FLAGS) -c -o $@ $<

.PHONY: clean

clean:
	rm -f $(OBJECTS) $(BIN)
