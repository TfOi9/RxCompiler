.DEFAULT_GOAL := test

include config.mk

PYTHON ?= python3
VERBOSE ?= false
# Directory paths below tests, joined with ':'; separate selections with ','.
FILTER ?=
COMPILE_TIMEOUT ?= 30
RUN_TIMEOUT ?= 10

# C++ compiler and ANTLR 4 C++ runtime configuration.
# ANTLR4_ROOT can be set explicitly when the runtime is installed outside the
# usual Homebrew/system locations.
ifeq ($(origin CXX),default)
CXX = c++
endif
CXXFLAGS += -std=c++17 -fPIC -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc
BUILD_DIR ?= build
ANTLR4_ROOT ?= $(shell \
	if command -v brew >/dev/null 2>&1 && brew --prefix antlr4-cpp-runtime >/dev/null 2>&1; then \
		brew --prefix antlr4-cpp-runtime; \
	fi)
ANTLR4_INCLUDE_DIR ?= $(if $(ANTLR4_ROOT),$(ANTLR4_ROOT)/include/antlr4-runtime,$(firstword $(wildcard /opt/homebrew/include/antlr4-runtime /usr/local/include/antlr4-runtime /usr/include/antlr4-runtime)))
ANTLR4_LIBRARY_DIR ?= $(if $(ANTLR4_ROOT),$(ANTLR4_ROOT)/lib,$(firstword $(wildcard /opt/homebrew/lib /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu)))
ANTLR4_CXXFLAGS ?= -I$(ANTLR4_INCLUDE_DIR)
ANTLR4_LDFLAGS ?= -L$(ANTLR4_LIBRARY_DIR)
ANTLR4_LIBS ?= -lantlr4-runtime
LINT_CXX ?= clang++

COMPILER_SOURCES := $(sort $(shell find src -type f -name '*.cpp' -print))
COMPILER_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(COMPILER_SOURCES))
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
COMPILER_LIBRARY := $(BUILD_DIR)/librxcompiler.dylib
SHARED_FLAGS := -dynamiclib
else
COMPILER_LIBRARY := $(BUILD_DIR)/librxcompiler.so
SHARED_FLAGS := -shared
endif

# Export commands as data, so shell quoting survives Make's recipe expansion.
export RX_TEST_BUILD = $(BUILD)
export RX_TEST_SEMANTIC = $(SEMANTIC)
export RX_TEST_CODEGEN = $(CODEGEN)
export RX_TEST_RUN = $(RUN)
export FILTER COMPILE_TIMEOUT RUN_TIMEOUT VERBOSE

.PHONY: build lint generate antlr4-runtime-check

antlr4-runtime-check:
	@if [ ! -f "$(ANTLR4_INCLUDE_DIR)/antlr4-runtime.h" ]; then \
		echo "ANTLR4 C++ runtime headers not found." >&2; \
		echo "Install antlr4-cpp-runtime or set ANTLR4_ROOT=/path/to/runtime." >&2; \
		exit 1; \
	fi
	@if [ ! -f "$(ANTLR4_LIBRARY_DIR)/libantlr4-runtime.a" ] && [ ! -f "$(ANTLR4_LIBRARY_DIR)/libantlr4-runtime.dylib" ] && [ ! -f "$(ANTLR4_LIBRARY_DIR)/libantlr4-runtime.so" ]; then \
		echo "ANTLR4 C++ runtime library not found in $(ANTLR4_LIBRARY_DIR)." >&2; \
		echo "Install antlr4-cpp-runtime or set ANTLR4_ROOT=/path/to/runtime." >&2; \
		exit 1; \
	fi

build: antlr4-runtime-check $(COMPILER_LIBRARY)

$(COMPILER_LIBRARY): $(COMPILER_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) $^ $(ANTLR4_LDFLAGS) $(ANTLR4_LIBS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ANTLR4_CXXFLAGS) -c $< -o $@

lint: antlr4-runtime-check
	$(LINT_CXX) $(CPPFLAGS) $(CXXFLAGS) $(ANTLR4_CXXFLAGS) -fsyntax-only $(COMPILER_SOURCES)

generate:
	@./scripts/generate_antlr_cpp.sh

.PHONY: test
test:
	@$(PYTHON) scripts/test.py
