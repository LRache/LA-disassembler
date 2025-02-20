CXX = clang++

EXAMPLE_DIR = example
BUILD_DIR = build

SRCS += $(shell find $(EXAMPLE_DIR) -name '*.cpp')
HEADERS += $(shell find include -name '*.h')

INC_PATH += include
INC_FLAGS = $(addprefix -I,$(INC_PATH))

CXXFLAGS += -std=c++17 -fPIC -Wall -Wextra -Werror -pedantic -O3 $(INC_FLAGS)

TARGET = $(BUILD_DIR)/example

example: $(TARGET)
	@ $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(info + CXX $@)
	@ $(CXX) $(CXXFLAGS) $(SRCS) $(LIB_TARGET) -o $(TARGET)

clean:
	$(info + CLEANING)
	@ rm -rf $(BUILD_DIR)
