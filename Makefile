PROGRAM := compiler.o

SOURCEDIR := src
BUILDDIR := build

CPPFILES := $(shell find $(SOURCEDIR) -type f -name '*.cpp')
HEADERS := $(shell find $(SOURCEDIR) -type f -name '*.(h|hpp)')
OBJ := $(CPPFILES:%.cpp=$(BUILDDIR)/%.o)

CXX := g++
CXXFLAGS := \
	-I./$(SOURCEDIR) \
	-c \
	-std=c++20 \

ifeq ($(TARGET), x86)
CXXFLAGS += -DCURR_ABI=3
else
CXXFLAGS += -DCURR_ABI=62
endif

.PHONY: all debug release clean
all:
	@echo "[!] No release type set"

debug: CXXFLAGS += -g -DDEBUG
debug: executable

release: CXXFLAGS += -Ofast -Wall
release: executable

executable: $(OBJ)
	$(CXX) $(OBJ) -o $(PROGRAM)

$(BUILDDIR)/%.o: %.cpp $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(BUILDDIR)/* $(PROGRAM)