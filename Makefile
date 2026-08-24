# Converter-C2S - C89 <-> Shalimar source converter.
#
# ISO C++14, and the same warning discipline the two compilers this answers
# to are built under. -Werror is not decoration: a converter that emits a
# language it cannot itself be trusted to compile under is not worth having.
#
# -MMD -MP is not optional here either. Compiler-S's CLAUDE.md puts it best:
# without header dependencies a stale object file is a heap corruptor, not a
# link error.

CXX      ?= c++
CXXFLAGS ?= -std=c++14 -O2 -g -Wall -Wextra -Werror -pedantic
DEPFLAGS := -MMD -MP

BINDIR ?= .
OBJDIR := obj
TARGET := $(BINDIR)/c2s.exe

SRCS := $(wildcard src/*.cpp) \
        $(wildcard src/c/*.cpp) \
        $(wildcard src/s/*.cpp) \
        $(wildcard src/convert/*.cpp)

OBJS := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# The two compilers are siblings of this tree and are used as oracles by the
# test suite. Nothing here compiles or links their sources - this converter is
# self-contained, and their repositories are not modified.
CC1 ?= ../Compiler-C/cc1.exe
SHC ?= ../Compiler-S/shc.exe

.PHONY: all clean test help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

test: $(TARGET)
	CC1=$(CC1) SHC=$(SHC) ./tests/run.sh

clean:
	rm -rf $(OBJDIR) $(TARGET)

help:
	@echo "make          build $(TARGET)"
	@echo "make test     build, then run the differential suite"
	@echo "make clean    remove $(OBJDIR) and the binary"
	@echo ""
	@echo "CC1=$(CC1)"
	@echo "SHC=$(SHC)"

-include $(DEPS)
