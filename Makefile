# Detect the operating system
UNAME_S := $(shell uname -s)

# Set compiler and flags based on the OS
ifeq ($(UNAME_S), Darwin)  # macOS
    LLVM_PATH := /opt/homebrew/opt/llvm@19/lib/
    CXX      := clang++
    LDFLAGS  := -L/usr/lib -L/opt/homebrew/lib -L$(LLVM_PATH) -lstdc++ -lm -lboost_system -lLLVMCore -lLLVMSupport -lLLVM-19
    INCLUDE  := -Iinclude/ -I/opt/homebrew/include
else  # Assume Linux
    CXX      := g++
    LDFLAGS  := -L/usr/lib -L/usr/local/lib -lstdc++ -lm -lboost_system
    INCLUDE  := -Iinclude/ -I/usr/local/include
endif

# Common flags
CXXFLAGS := -std=c++23
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

-include $(DEPENDENCIES)

.PHONY: all build clean debug release info

build:
	@mkdir -p $(APP_DIR)
	@mkdir -p $(OBJ_DIR)

debug: CXXFLAGS += -DDEBUG -g
debug: all

release: CXXFLAGS += -O2
release: all

clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(APP_DIR)/*

info:
	@echo "[*] Detected OS:      ${UNAME_S}     "
	@echo "[*] Application dir:  ${APP_DIR}     "
	@echo "[*] Object dir:       ${OBJ_DIR}     "
	@echo "[*] Sources:          ${SRC}         "
	@echo "[*] Objects:          ${OBJECTS}     "
	@echo "[*] Dependencies:     ${DEPENDENCIES}"
