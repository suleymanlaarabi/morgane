# Development shortcuts over the CMake presets in CMakePresets.json.
#   make                      build, then run the server and the client
#   make build                configure if needed and build
#   make PRESET=linux-release run

PRESET ?= linux-debug
JOBS ?= 4

BUILD_DIR := build/$(PRESET)
BIN_DIR := $(BUILD_DIR)/bin
VCPKG := .tools/vcpkg/vcpkg

SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := run

.PHONY: run run-server run-client build test clean distclean bootstrap help

$(VCPKG):
	./scripts/bootstrap.sh

$(BUILD_DIR)/CMakeCache.txt: $(VCPKG)
	cmake --preset $(PRESET)

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build --preset $(PRESET) --parallel $(JOBS)

# The server runs in the background and is stopped when the client exits.
run: build
	@$(BIN_DIR)/r-type_server & server_pid=$$!; \
	trap 'kill $$server_pid 2>/dev/null || true' EXIT INT TERM; \
	$(BIN_DIR)/r-type_client

run-server: build
	$(BIN_DIR)/r-type_server

run-client: build
	$(BIN_DIR)/r-type_client

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
	@echo "  run          build, then start the server and the client (default)"
	@echo "  run-server   build, then start the server only"
	@echo "  run-client   build, then start the client only"
	@echo "  build        configure if needed and build"
	@echo "  test         build and run ctest"
	@echo "  clean        clean build outputs"
	@echo "  distclean    remove the $(BUILD_DIR) tree"
	@echo "  bootstrap    prepare the pinned vcpkg tool"
	@echo ""
	@echo "Variables: PRESET=$(PRESET) JOBS=$(JOBS)"
