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

# Where the two repositories are, as opposed to the two binaries. Only the
# vendor rules below need this; everything else runs the compilers rather
# than reading them.
SHC_DIR ?= ../Compiler-S

# src/s/vendor is a copy of Compiler-S's front end, and a copy is a snapshot.
# The C89 side of this converter drifts visibly - the suite runs the real cc1
# over every case, so a divergence fails something. This side drifts
# invisibly: a change in Compiler-S's parser leaves the copy stale and
# nothing here would notice unless a case happened to straddle the
# difference. Hence a rule to refresh it and a check that says when it needs
# refreshing.
#
# These fourteen are the whole of what the converter uses - the minimum front
# end docs/ANALYSIS.md section 4 identified by include-graph, plus its
# headers. Nothing in Compiler-S's backend, Resolve, CodeGen or Target is
# here, and nothing should be.
VENDOR := Ast.cpp Ast.h Builtin.cpp Builtin.h Check.cpp Check.h \
          Diag.cpp Diag.h Lexer.cpp Parser.cpp Parser.h Token.h \
          Type.cpp Type.h

.PHONY: all clean test help rpkg vendor vendor-check

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

test: $(TARGET) vendor-check
	CC1=$(CC1) SHC=$(SHC) ./tests/run.sh

# Re-copy the Shalimar front end from Compiler-S. This is the whole of what
# "upgrade Shalimar and the converter follows" means here: change that
# repository, run this, run the suite.
#
# It is worth knowing what this does not carry. These files are the front
# end - what Shalimar *accepts*. What the converter can *emit* lives in
# src/s/SPrinter.cpp and the two mappings in src/convert/, which are this
# repository's own and which no copy updates. So a fix or a tightened rule
# arrives with this; a brand-new kind of AST node does not, and will instead
# fail this tree's build the moment the copy is refreshed - because SPrinter
# implements the vendored NodeVisitor, and a new pure virtual is a compile
# error rather than a silent mistranslation. That is the good outcome, and
# it only happens if the copy is current.
# The copies are mode 444 on purpose, so that editing one is a permission
# error rather than a change nobody notices - the whole point of a vendored
# file is that it is not this repository's to alter. That means the copy has
# to be removed before it can be replaced, and the mode put back afterwards;
# a plain cp fails outright, which is how this rule was found to need saying.
# (git records only the executable bit, so a fresh clone arrives writable and
# this rule is what restores the invariant.)
vendor:
	@for f in $(VENDOR); do \
	    if [ ! -f $(SHC_DIR)/src/$$f ]; then \
	        echo "vendor: no $(SHC_DIR)/src/$$f"; exit 1; \
	    fi; \
	    rm -f src/s/vendor/$$f; \
	    cp $(SHC_DIR)/src/$$f src/s/vendor/$$f; \
	    chmod a-w src/s/vendor/$$f; \
	done
	@echo "vendor: src/s/vendor is now level with $(SHC_DIR)/src"
	@echo "        rebuild and run the suite before trusting it"

# Named by `test`, so a stale copy is reported by the thing that would
# otherwise pass without noticing. A missing sibling is not a failure - it
# is the ordinary state of a tree built on its own - but it is said out
# loud, because a check that quietly did not run is the shape of green this
# project does not accept.
vendor-check:
	@if [ ! -d $(SHC_DIR)/src ]; then \
	    echo "vendor: $(SHC_DIR)/src is not here - drift NOT checked"; \
	else \
	    drift=0; \
	    for f in $(VENDOR); do \
	        if ! cmp -s src/s/vendor/$$f $(SHC_DIR)/src/$$f; then \
	            echo "vendor: src/s/vendor/$$f differs from $(SHC_DIR)/src/$$f"; \
	            drift=`expr $$drift + 1`; \
	        fi; \
	    done; \
	    if [ $$drift -ne 0 ]; then \
	        echo "vendor: $$drift file(s) have drifted from Compiler-S."; \
	        echo "        'make vendor' refreshes them; then rebuild and retest."; \
	        exit 1; \
	    fi; \
	    echo "vendor: the Shalimar front end is level with $(SHC_DIR)/src"; \
	fi

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
	@echo "make vendor   re-copy the Shalimar front end from $(SHC_DIR)/src"
	@echo "make rpkg     copy the engine into rstudio/c2sr for R CMD INSTALL"
	@echo "make clean    remove $(OBJDIR), the binary and the copied engine"
	@echo ""
	@echo "CC1=$(CC1)"
	@echo "SHC=$(SHC)"

-include $(DEPS)
