PROGRAMNAME := compiler.o
BUILDDIR := ../build/
SOURCEDIR := ./

SOURCES := $(shell find "./" -type f -name '*.cpp')
HEADERS := $(shell find "./" -type f -name '*.(h|hpp)')
OBJS := $(subst $(SOURCEDIR), $(BUILDDIR), $(SOURCES))
OBJS := $(OBJS:.cpp=.o)

CXX := clang++
CXXFLAGS := \
	-I./ \
	-c \
	-std=c++20 \
	-MMD \
	-Wno-switch \
	`$(llvm-config --cxxflags)`

#ifeq ($(TARGET), x86)
#CXXFLAGS += -DCURR_ABI=3
#else
#CXXFLAGS += -DCURR_ABI=62
#endif
LDFLAGS := `llvm-config --ldflags` -v -rdynamic
LIBS := `llvm-config --libs --system-libs`

.PHONY: all debug release clean

all:
	@echo "[!] No release type set"

debug: CXXFLAGS += -g -DDEBUG

release: CXXFLAGS += -O3

debug: executable
release: executable

executable: $(OBJS)
	@echo -n LINKING EVERYTHING...
	@$(CXX) $(LDFLAGS) $(LIBS) $(foreach obj, $(OBJS), $(BUILDDIR)/$(obj)) -o ../$(PROGRAMNAME)
	@echo done

$(BUILDDIR)%.o: $(SOURCEDIR)%.cpp
	@echo [C++] COMPILING $<
	@mkdir -p $(BUILDDIR)/$(@D)
	@$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/$(@D)/$(notdir $@) $<

clean:
	@rm -rf $(BUILDDIR)/* $(PROGRAM)