CXX = g++
CXXFLAGS_COMMON = -Wall -Wextra -std=c++17
LDFLAGS = -lrt -lpthread

# Default: release
CXXFLAGS = $(CXXFLAGS_COMMON) -O2

TARGETS = bank_init bank_deinit client_local server client_net

.PHONY: all release debug test valgrind coverage clean

all: release

release: CXXFLAGS = $(CXXFLAGS_COMMON) -O2
release: $(TARGETS)

debug: CXXFLAGS = $(CXXFLAGS_COMMON) -g -O0
debug: $(TARGETS)

# Object files
bank.o: bank.cpp bank.h
	$(CXX) $(CXXFLAGS) -c bank.cpp -o bank.o

bank_init.o: bank_init.cpp bank.h
	$(CXX) $(CXXFLAGS) -c bank_init.cpp -o bank_init.o

bank_deinit.o: bank_deinit.cpp bank.h
	$(CXX) $(CXXFLAGS) -c bank_deinit.cpp -o bank_deinit.o

client_local.o: client_local.cpp bank.h
	$(CXX) $(CXXFLAGS) -c client_local.cpp -o client_local.o

server.o: server.cpp bank.h
	$(CXX) $(CXXFLAGS) -c server.cpp -o server.o

client_net.o: client_net.cpp colorprint.h
	$(CXX) $(CXXFLAGS) -c client_net.cpp -o client_net.o

# Executables
bank_init: bank_init.o bank.o
	$(CXX) $(CXXFLAGS) bank_init.o bank.o -o bank_init $(LDFLAGS)

bank_deinit: bank_deinit.o bank.o
	$(CXX) $(CXXFLAGS) bank_deinit.o bank.o -o bank_deinit $(LDFLAGS)

client_local: client_local.o bank.o
	$(CXX) $(CXXFLAGS) client_local.o bank.o -o client_local $(LDFLAGS)

server: server.o bank.o
	$(CXX) $(CXXFLAGS) server.o bank.o -o server $(LDFLAGS)

client_net: client_net.o
	$(CXX) $(CXXFLAGS) client_net.o -o client_net $(LDFLAGS)

# Tests
test: debug
	bash tests/test_basic.sh
	bash tests/test_concurrent.sh

# Valgrind
valgrind: debug
	bash tests/test_valgrind.sh

# Coverage
coverage: CXXFLAGS = $(CXXFLAGS_COMMON) -g -O0 --coverage
coverage: LDFLAGS += --coverage
coverage: clean $(TARGETS)
	bash tests/test_basic.sh
	@echo "=== Coverage Report ==="
	gcov bank.cpp
	@echo "See *.gcov files for details"

clean:
	rm -f *.o *.gcno *.gcda *.gcov $(TARGETS)
