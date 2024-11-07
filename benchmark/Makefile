#
# Simple makefile by __ e k L 1 p 5 3 d __
#

# Variables
SRCS	:=./benchmark.c ./benchmark_ekjson.c ./benchmark_jsmn.c ./benchmark_jjson.c ./benchmark_jsonc.c ./ekjson/src/ekjson.c
CXXSRCS	:=./benchmark_simdjson.cpp ./simdjson/singleheader/simdjson.cpp ./benchmark_rapidjson.cpp
BUILD	:=build
OUT	:=$(BUILD)/benchmark
OBJS	:=$(patsubst %.c,$(BUILD)/%.o,$(SRCS))
CXXOBJS	:=$(patsubst %.cpp,$(BUILD)/%.o,$(CXXSRCS))

# Environment variables
FLAGS	:=-O2 -Isimdjson/singleheader -Ijjson/extern/array/include -Ijjson/extern/hash-cache/include -Ijjson/extern/dict/include -Ijjson/extern/log/include -Ijjson/extern/sync/include -Ijjson/include -Irapidjson/include/
LDFLAGS	:=$(LDFLAGS) -lm -Ljjson/lib/ -larray -ldict -ljson -lhash_cache -llog -lsync -Ljson-c/ -ljson-c

CFLAGS	:=$(CFLAGS) $(FLAGS) -std=gnu99
CXXFLAGS:=$(CXXFLAGS) $(FLAGS) -std=c++11

# Build the main executable
$(OUT): $(OBJS) $(CXXOBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)
	@if [[ $$(uname) == "Darwin" ]]; then\
		install_name_tool -add_rpath $$PWD/jjson/lib $(OUT);\
		install_name_tool -add_rpath $$PWD/json-c $(OUT);\
	fi

# Remember that when this call is evaluated, it is expanded TWICE!
define COMPILE
$$(BUILD)/$(dir $(2))$(1)
	mkdir -p $$(dir $$@)
	$$(CC) -c $(2) $$(CFLAGS) -o $$@
endef

# Go through every source file use gcc to find its pre-reqs and create a rule
$(foreach src,$(SRCS),$(eval $(call COMPILE,$(shell $(CC) $(CFLAGS) -M $(src) | tr -d '\\'),$(src))))

# Remember that when this call is evaluated, it is expanded TWICE!
define CXXCOMPILE
$$(BUILD)/$(dir $(2))$(1)
	mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXFLAGS) -c $(2) -o $$@
endef

# Go through every source file use gcc to find its pre-reqs and create a rule
$(foreach src,$(CXXSRCS),$(eval $(call CXXCOMPILE,$(shell $(CXX) $(CXXFLAGS) -M $(src) | tr -d '\\'),$(src))))

# Build Dependencies
dependencies:
	@git submodule update --init --recurse
	@cd jjson/
	@cmake -DCMAKE_BUILD_TYPE="Release" .
	@make
	@cd ../json-c
	@cmake .
	@make
	@cd ../
	@make

# Clean the project directory
.PHONY: clean
clean:
	rm -rf $(BUILD)

