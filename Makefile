CXX=g++
CXXFLAGS=-std=c++17 -O2 -pthread
BIN=bin/island
SRC=src/island.cpp src/semaphore.cpp

all: $(BIN)

$(BIN): $(SRC)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC)

# 7 adults 9 children
run: $(BIN)
	$(BIN) 7 9 

clean:
	rm -rf bin