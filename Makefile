CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
SOURCES := $(wildcard src/*.cpp)
cadd0000: $(SOURCES) include/model.hpp include/stage2.hpp
	$(CXX) $(CXXFLAGS) -Iinclude $(SOURCES) -o $@
test: cadd0000
	python3 tests/test_stage2.py ./cadd0000
	$(CXX) $(CXXFLAGS) -Iinclude tests/test_operations.cpp $(filter-out src/main.cpp,$(SOURCES)) -o test_operations
	./test_operations examples/pdf_demo
clean:
	rm -f cadd0000 test_operations
.PHONY: test clean
