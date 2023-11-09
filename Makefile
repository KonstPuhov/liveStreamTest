DEBUG ?= DEBUG
VERSION = $(shell git describe)

CXX = g++
CXXFLAGS = -Wall -Werror -Wextra -pedantic -std=c++17 -g -fsanitize=address -ISrc/Common -D$(DEBUG) -D_VERSION_=\"$(VERSION)\"
LDFLAGS =  -fsanitize=address

H_COM = $(shell find Src/Common -name '*.h')
SRC_S = $(shell find Src/Server -name '*.cpp')
SRC_S += $(shell find Src/Common -name '*.cpp')
H_S = $(shell find Src/Server -name '*.h')
SRC_C = $(shell find Src/Client -name '*.cpp')
SRC_C += $(shell find Src/Common -name '*.cpp')
H_C = $(shell find Src/Client -name '*.h')
OBJ_S = $(SRC_S:.cpp=.o)
OBJ_C = $(SRC_C:.cpp=.o)
EXEC_S = server
EXEC_C = client

.PHONY: all run clean
all: $(EXEC_S) $(EXEC_C)

$(OBJ_S): $(SRC_S) $(H_S) $(H_COM) Makefile
$(OBJ_C): $(SRC_C) $(H_C) $(H_COM) Makefile

$(EXEC_S): $(OBJ_S)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ_S) $(LBLIBS)

$(EXEC_C): $(OBJ_C)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ_C) $(LBLIBS)

clean:
	rm -rf $(OBJ_S) $(EXEC_S) $(OBJ_C) $(EXEC_C) *.bin

run: all
	./server -b1000 &
	./client -fmyDog.jpg -c3211 &
	./client -fmyCat.jpg -c3212
	killall server