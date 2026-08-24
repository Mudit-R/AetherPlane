CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -Wall -Wextra -Iinclude -pthread

BUILD_DIR = build
BIN = $(BUILD_DIR)/aetherplane_daemon

SRCS = src/core/packet.cpp \
       src/core/lpm_trie.cpp \
       src/core/filter_engine.cpp \
       src/core/xdp_hook.cpp \
       src/core/virtual_netdev.cpp \
       src/qos/flow_classifier.cpp \
       src/qos/smart_scheduler.cpp \
       src/telemetry/metrics_collector.cpp \
       src/main.cpp

OBJS = $(patsubst src/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

.PHONY: all clean test bench run dashboard docker

all: $(BIN)

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

test:
	python3 tests/test_data_path.py

train-ml:
	python3 ml/train_qos_model.py

bench:
	python3 benchmarks/benchmark_runner.py

dashboard:
	python3 server/app.py

docker:
	docker-compose up --build

clean:
	rm -rf $(BUILD_DIR)
