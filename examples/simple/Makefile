
CC := g++
FLAGS := -Wall -std=c++1y -fPIC -pie -rdynamic -I../../include -O0 -g

OBJECTS := simple.o
BIN := simple

HEADERS := ../../include/ict/
LIBRARIES := -ldl -lecl -lgc

$(BIN): $(OBJECTS)
	$(CC) $(FLAGS) -o $@ $(OBJECTS) $(LIBRARIES)

$(OBJECTS): %.o: %.c++ $(HEADERS)
	$(CC) $(FLAGS) -c -o $@ $<

.PHONY: clean

clean:
	rm -f $(OBJECTS) $(BIN)
