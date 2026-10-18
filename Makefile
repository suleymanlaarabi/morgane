# Development shortcuts over the CMake presets in CMakePresets.json.
#   make                      build, then run the server and a client on its menu
#   make server               run the server only
#   make client               run a client on its menu, to join a server by hand
#   make coop PLAYERS=3       run the server and several clients that join it at once
#   make PRESET=linux-release run

PRESET ?= linux-debug
JOBS ?= 4
PORT ?= 4242
ADDRESS ?= 127.0.0.1:$(PORT)
PLAYERS ?= 2

BUILD_DIR := build/$(PRESET)
BIN_DIR := $(BUILD_DIR)/bin
VCPKG := .tools/vcpkg/vcpkg

SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := run

.PHONY: run server client coop run-server run-client build test clean distclean bootstrap help

$(VCPKG):
	./scripts/bootstrap.sh

$(BUILD_DIR)/CMakeCache.txt: $(VCPKG)
	cmake --preset $(PRESET)

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build --preset $(PRESET) --parallel $(JOBS)

# The server runs in the background and is stopped when the client exits.
run: build
	@$(BIN_DIR)/r-type_server $(PORT) & server_pid=$$!; \
	trap 'kill $$server_pid 2>/dev/null || true' EXIT INT TERM; \
	$(BIN_DIR)/r-type_client $(ADDRESS)

server: build
	$(BIN_DIR)/r-type_server $(PORT)

client: build
	$(BIN_DIR)/r-type_client $(ADDRESS)

# Every process stops once the last client window is closed.
coop: build
	@[ $(PLAYERS) -ge 1 ] || { echo "coop needs PLAYERS=1 or more" >&2; exit 2; }; \
	$(BIN_DIR)/r-type_server $(PORT) & server_pid=$$!; clients=; \
	trap 'kill $$server_pid $$clients 2>/dev/null || true' EXIT INT TERM; \
	for i in $$(seq $(PLAYERS)); do $(BIN_DIR)/r-type_client $(ADDRESS) --join & clients="$$clients $$!"; done; \
	wait $$clients

run-server: server
run-client: client

test: build
	ctest --preset $(PRESET)

clean:
	cmake --build --preset $(PRESET) --target clean

distclean:
	rm -rf $(BUILD_DIR)

bootstrap:
	./scripts/bootstrap.sh

help:
	@echo "Targets:"
	@echo "  run          build, then start the server and a client on its menu (default)"
	@echo "  server       build, then start the server only"
	@echo "  client       build, then start a client on its menu"
	@echo "  coop         build, then start the server and PLAYERS clients that join it"
	@echo "  build        configure if needed and build"
	@echo "  test         build and run ctest"
	@echo "  clean        clean build outputs"
	@echo "  distclean    remove the $(BUILD_DIR) tree"
	@echo "  bootstrap    prepare the pinned vcpkg tool"
	@echo ""
	@echo "Variables: PRESET=$(PRESET) JOBS=$(JOBS) PORT=$(PORT) ADDRESS=$(ADDRESS) PLAYERS=$(PLAYERS)"
