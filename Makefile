C               := gcc
C_FLAGS := -ggdb

BIN             := bin
SRC             := src
INCLUDE         := include

LIBRARIES       :=
EXECUTABLE      := main

TEST		:= testcases

all: $(BIN)/$(EXECUTABLE)

run: clean all
	clear
	./$(BIN)/$(EXECUTABLE)

$(BIN)/$(EXECUTABLE): $(SRC)/*.c
	$(C) $(C_FLAGS) -I$(INCLUDE) $^ -o $@ $(LIBRARIES)

clean:
	rm $(BIN)/*

test: $(SRC)/*.c $(TEST)/$(case).c
	$(C) $(C_FLAGS) -I$(INCLUDE) $^ -o $@ $(LIBRARIES)
	mv test $(BIN)/$(case)
