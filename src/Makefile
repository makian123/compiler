PROGRAMNAME := compiler.o
BUILDDIR := ../build/
SOURCEDIR := ./

SOURCES := $(shell find "./" -type f -name '*.cpp')
HEADERS := $(shell find "./" -type f -name '*.(h|hpp)')
OBJS := $(subst $(SOURCEDIR), $(BUILDDIR), $(SOURCES))
OBJS := $(OBJS:.cpp=.o)

CXX := g++
CXXFLAGS := \
	-I./ \
	-c \
	-std=c++20 \
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

release: CXXFLAGS += -Ofast

debug: executable
release: executable

executable: $(OBJS)
	@echo -n LINKING EVERYTHING...
	@$(CXX) $(LDFLAGS) $(foreach obj, $(OBJS), $(BUILDDIR)/$(obj)) -o ../$(PROGRAMNAME)
	@echo done

$(BUILDDIR)%.o: $(SOURCEDIR)%.cpp
	@echo [C++] COMPILING $<
	@mkdir -p $(BUILDDIR)/$(@D)
	@$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/$(@D)/$(notdir $@) $<

clean:
	@rm -rf $(BUILDDIR)/* $(PROGRAM)