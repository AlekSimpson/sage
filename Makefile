# Detect the operating system
UNAME_S := $(shell uname -s)

# Set compiler and flags based on the OS
ifeq ($(UNAME_S), Darwin)  # macOS
    LLVM_PATH := /opt/homebrew/opt/llvm@19/lib/
    LLVM_INCLUDE := /opt/homebrew/Cellar/llvm/19.1.7_1/include
    CXX      := clang++ -stdlib=libc++
    LDFLAGS  := -L/usr/lib -L/opt/homebrew/lib -L$(LLVM_PATH) -lstdc++ -lm -lboost_system -lLLVM-19
    INCLUDE  := -Iinclude/ -I/opt/homebrew/include -I$(LLVM_INCLUDE)
    DEBUG_FLAGS := -gdwarf-4 -fno-omit-frame-pointer -fstandalone-debug
else  # Assume Linux
    LLVM_PATH := /usr/lib/llvm-14/lib
    CXX      := g++
    LDFLAGS  := -L/usr/lib -L/usr/local/lib -L$(LLVM_PATH) $(llvm-config --ldflags --libs core remarks debuginfodwarf support) -lstdc++ -lm -lboost_system -lLLVM-14 -ltinfo
    INCLUDE  := -Iinclude/ -I/usr/local/include -I/usr/lib/llvm-14/include
    DEBUG_FLAGS := -gdwarf-4 -fno-omit-frame-pointer
endif

# Common flags
CXXFLAGS := -Wall -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
BUILD    := ./build
OBJ_DIR  := $(BUILD)/objects
APP_DIR  := $(BUILD)/bin
TARGET   := sage
SRC      := $(wildcard src/*.cpp)
OBJECTS  := $(SRC:%.cpp=$(OBJ_DIR)/%.o)
DEPENDENCIES := $(OBJECTS:.o=.d)

# Targets
all: build $(APP_DIR)/$(TARGET)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -MMD -o $@

$(APP_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $(APP_DIR)/$(TARGET) $^ $(LDFLAGS)
ifeq ($(UNAME_S), Darwin)
ifdef DEBUG_BUILD
	dsymutil $(APP_DIR)/$(TARGET)
endif
endif

-include $(DEPENDENCIES)

.PHONY: all build clean debug release info rebuild-debug memdebug test test-update create-test

build:
	@mkdir -p $(APP_DIR)
	@mkdir -p $(OBJ_DIR)

test: $(APP_DIR)/$(TARGET)
	julia tests/run_tests.jl

test-update: $(APP_DIR)/$(TARGET)
	julia tests/run_tests.jl --update

create-test: $(APP_DIR)/$(TARGET)
ifndef NAME
	$(error NAME is required. Usage: make create-test NAME=my_test_name)
endif
	julia tests/run_tests.jl --create $(NAME)

# Memory debugging target with Address and Leak sanitizers
memdebug: CXXFLAGS += $(DEBUG_FLAGS) -fsanitize=address,leak
memdebug: LDFLAGS += -fsanitize=address,leak
memdebug: DEBUG_BUILD=1
memdebug: all

debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: DEBUG_BUILD=1
debug: all

release: CXXFLAGS += -O2
release: all

clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(APP_DIR)/*
	-@rm -rvf $(APP_DIR)/$(TARGET).dSYM

rebuild-debug: clean debug

info:
	@echo "[*] Detected OS:      ${UNAME_S}     "
	@echo "[*] Application dir:  ${APP_DIR}     "
	@echo "[*] Object dir:       ${OBJ_DIR}     "
	@echo "[*] Sources:          ${SRC}         "
	@echo "[*] Objects:          ${OBJECTS}     "
	@echo "[*] Dependencies:     ${DEPENDENCIES}"
