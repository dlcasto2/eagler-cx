# Host-side unit tests for Crafti's pure-logic code.
# Paths in HOST_SRCS are relative to the repository root.
CXX ?= g++
CXXFLAGS = -std=c++11 -fno-rtti -Wall -Wextra -g -I.. -I../nGL -I.
HOST_SRCS = gameclock.cpp items.cpp blockdefs.cpp playerinventory.cpp physics.cpp entity.cpp aabb.cpp saveio.cpp blockstorage.cpp itempile.cpp gamestate.cpp savefile.cpp player_motion.cpp nGL/fastmath.cpp lighting.cpp stresstest.cpp screenmove.cpp daylight.cpp mining.cpp drops.cpp
TEST_SRCS = $(wildcard test_*.cpp)
EXTRA_SRCS = ngl_stubs.cpp

run_tests: test_main.cpp $(TEST_SRCS) $(EXTRA_SRCS) $(addprefix ../,$(HOST_SRCS))
	$(CXX) $(CXXFLAGS) $^ -lz -o $@

test: run_tests
	./run_tests

.PHONY: test
