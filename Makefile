CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -fPIC -pthread -Isrc
LDFLAGS = -L$(BUILD_DIR) -lmultilogger -Wl,-rpath,$(BUILD_DIR)

BUILD_DIR = ./build

LIB_TARGET = $(BUILD_DIR)/libmultilogger.so
APP_TARGET = $(BUILD_DIR)/logger_app
TEST_TARGET = $(BUILD_DIR)/logger_test

LIB_SRCS = $(wildcard ./src/lib/*.cpp)
APP_SRCS = $(wildcard ./src/app/*.cpp)
TEST_SRCS = $(wildcard ./test/*.cpp)

LIB_OBJS = $(patsubst ./src/lib/%.cpp, $(BUILD_DIR)/lib/%.o, $(LIB_SRCS))
APP_OBJS = $(patsubst ./src/app/%.cpp, $(BUILD_DIR)/app/%.o, $(APP_SRCS))
TEST_OBJS = $(patsubst ./test/%.cpp, $(BUILD_DIR)/test/%.o, $(TEST_SRCS))

.PHONY: all clean test

all: $(LIB_TARGET) $(APP_TARGET) $(TEST_TARGET)

$(LIB_TARGET): $(LIB_OBJS)
	$(CXX) -shared -o $@ $^

$(APP_TARGET): $(APP_OBJS) $(LIB_TARGET)
	$(CXX) $(CXXFLAGS) -o $@ $(APP_OBJS) $(LDFLAGS)

$(TEST_TARGET): $(TEST_OBJS) $(LIB_TARGET)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJS) $(LDFLAGS)

test: $(TEST_TARGET) $(APP_TARGET)
	$(TEST_TARGET)

$(BUILD_DIR)/lib/%.o: ./src/lib/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/app/%.o: ./src/app/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test/%.o: ./test/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -fr $(BUILD_DIR)/app
	rm -fr $(BUILD_DIR)/lib
	rm -fr $(BUILD_DIR)/test