
CC := g++
FLAGS := -Wall -std=c++1y -fPIC -pie -rdynamic -I../../include -O0 -g

OBJECTS := simple.o
BIN := simple

LIBRARIES := -ldl -lecl -lgc

$(BIN): $(OBJECTS)
	$(CC) $(FLAGS) -o $@ $(OBJECTS) $(LIBRARIES)

$(OBJECTS): %.o: %.c++
	$(CC) $(FLAGS) -c -o $@ $<

.PHONY: clean

clean:
	rm -f $(OBJECTS) $(BIN)
