CXX      ?= g++
AR       ?= ar
CXXFLAGS ?= -std=c++14 -O2 -Wall -Wextra -pedantic
CPPFLAGS  = -Iinclude -Idata
LDFLAGS  ?=

# NEON=0 disables kernels. NEON=1 enables them when the compiler defines __ARM_NEON.
# 32-bit ARM cross-builds need an FPU flag, e.g. make NEON=1 ARM_FPU=neon CXX=arm-linux-gnueabihf-g++
ifeq ($(NEON),0)
CPPFLAGS += -DKEEB_ENABLE_NEON=0
endif
ifeq ($(NEON),1)
CPPFLAGS += -DKEEB_ENABLE_NEON=1
endif
ifneq ($(ARM_FPU),)
CXXFLAGS += -mfpu=$(ARM_FPU)
endif

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

cli: lib keeb analyze_wav bpm_probe key_probe run_benchmark

keeb: cli/keeb.cpp libkeeb.a
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -Icli $< -o $@ libkeeb.a $(LDFLAGS)

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
	rm -f $(OBJ) libkeeb.a keeb analyze_wav bpm_probe key_probe run_benchmark keeb_unit_tests keeb_integration_tests
