CXX      ?= g++
AR       ?= ar
CXXFLAGS ?= -std=c++14 -O2 -Wall -Wextra -pedantic
CPPFLAGS  = -Iinclude -Idata
LDFLAGS  ?=

SRC = src/wav_io.cpp src/dsp.cpp src/fft.cpp src/onset.cpp \
      src/tempo.cpp src/chroma.cpp src/key.cpp src/api.cpp \
      src/stream.cpp src/c_api.cpp
OBJ = $(SRC:.cpp=.o)

.PHONY: all lib cli tests clean

all: lib

lib: libkeeb.a

libkeeb.a: $(OBJ)
	$(AR) rcs $@ $^

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

cli: lib analyze_wav bpm_probe key_probe run_benchmark

analyze_wav: cli/analyze_wav.cpp libkeeb.a
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -Icli $< -o $@ libkeeb.a $(LDFLAGS)

bpm_probe: cli/bpm_probe.cpp libkeeb.a
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -Icli $< -o $@ libkeeb.a $(LDFLAGS)

key_probe: cli/key_probe.cpp libkeeb.a
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -Icli $< -o $@ libkeeb.a $(LDFLAGS)

run_benchmark: cli/run_benchmark.cpp libkeeb.a
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -Icli -Itests/unit $< -o $@ libkeeb.a $(LDFLAGS)

tests: lib keeb_unit_tests keeb_integration_tests
	./keeb_unit_tests
	./keeb_integration_tests

keeb_unit_tests: tests/unit/test_all.cpp libkeeb.a
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -Ithird_party -Itests/unit $< -o $@ libkeeb.a $(LDFLAGS)

keeb_integration_tests: tests/integration/test_integration.cpp libkeeb.a
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -Ithird_party -Itests/unit -Itests/integration $< -o $@ libkeeb.a $(LDFLAGS)

clean:
	rm -f $(OBJ) libkeeb.a analyze_wav bpm_probe key_probe run_benchmark keeb_unit_tests keeb_integration_tests
