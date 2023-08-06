PROGRAMNAME := compiler.o
BUILDDIR := ../build

SOURCES := $(shell find "./" -type f -name '*.cpp')
HEADERS := $(shell find "./" -type f -name '*.(h|hpp)')
OBJ = $(SOURCES:.cpp=.o)
DEP = 

CXX := g++
CXXFLAGS := \
	-I./ \
	-c \
	-std=c++20 \
	-Wall \
	-Wno-stringop-truncation \
	-MMD

ifeq ($(TARGET), x86)
CXXFLAGS += -DCURR_ABI=3
else
CXXFLAGS += -DCURR_ABI=62
endif
LDFLAGS := 

.PHONY: all debug release clean

all:
	@echo "[!] No release type set"

debug: CXXFLAGS += -g -DDEBUG
debug: BUILDDIR = ../build/debug
debug: DEP = $(shell find $(BUILDDIR) -type f -name '*.d')

release: CXXFLAGS += -Ofast
release: BUILDDIR = ../build/release
release: DEP = $(shell find $(BUILDDIR) -type f -name '*.d')

debug: executable
release: executable

executable: $(OBJ) $(HEADERS)
	@echo -n LINKING EVERYTHING...
	@$(CXX) $(LDFLAGS) $(foreach obj, $(OBJ), $(BUILDDIR)/$(obj)) -o ../$(PROGRAMNAME)
	@echo done



%.o: %.cpp
	@echo [C++] COMPILING $<
	@mkdir -p $(BUILDDIR)/$(@D)
	@$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/$(@D)/$(notdir $@) $<

clean:
	@rm -rf $(BUILDDIR)/* $(PROGRAM)