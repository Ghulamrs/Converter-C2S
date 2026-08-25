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

# src/s/vendor is the Shalimar front end carried over from Compiler-S. It is
# a copy, not a link - see CLAUDE.md - and it has to be compiled here like
# everything else, or every shalimar:: symbol goes missing at the link.
SRCS := $(wildcard src/*.cpp) \
        $(wildcard src/c/*.cpp) \
        $(wildcard src/s/*.cpp) \
        $(wildcard src/s/vendor/*.cpp) \
        $(wildcard src/convert/*.cpp)

OBJS := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# The two compilers are siblings of this tree and are used as oracles by the
# test suite. Nothing here compiles or links their sources - this converter is
# self-contained, and their repositories are not modified.
CC1 ?= ../Compiler-C/cc1.exe
SHC ?= ../Compiler-S/shc.exe

.PHONY: all clean test help rpkg

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

test: $(TARGET)
	CC1=$(CC1) SHC=$(SHC) ./tests/run.sh

# Assemble the RStudio package: the engine sources are copied under the
# package's src/core/ (main.cpp excluded - R owns the entry point), so
# R CMD INSTALL rstudio/c2sr builds the whole converter for R.
#
# rstudio/c2sr/src/Makevars names this target and will not build without it,
# so the two have to stay together. They did not: the target was written,
# the Makevars note pointing at it was committed, and the target itself
# never was - it survived only in a deploy archive, which is where this copy
# came back from.
rpkg:
	rm -rf rstudio/c2sr/src/core
	mkdir -p rstudio/c2sr/src/core/c rstudio/c2sr/src/core/s/vendor \
	         rstudio/c2sr/src/core/convert
	cp src/*.h rstudio/c2sr/src/core/
	cp $(filter-out src/main.cpp,$(wildcard src/*.cpp)) rstudio/c2sr/src/core/
	cp src/c/*.h src/c/*.cpp rstudio/c2sr/src/core/c/
	cp src/s/*.h src/s/*.cpp rstudio/c2sr/src/core/s/
	cp src/s/vendor/*.h src/s/vendor/*.cpp rstudio/c2sr/src/core/s/vendor/
	cp src/convert/*.h src/convert/*.cpp rstudio/c2sr/src/core/convert/
	@echo "rstudio/c2sr is ready: R CMD INSTALL rstudio/c2sr"

clean:
	rm -rf $(OBJDIR) $(TARGET) rstudio/c2sr/src/core

help:
	@echo "make          build $(TARGET)"
	@echo "make test     build, then run the differential suite"
	@echo "make rpkg     copy the engine into rstudio/c2sr for R CMD INSTALL"
	@echo "make clean    remove $(OBJDIR), the binary and the copied engine"
	@echo ""
	@echo "CC1=$(CC1)"
	@echo "SHC=$(SHC)"

-include $(DEPS)
