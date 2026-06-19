PSP_CONFIG ?= psp-config
PSP_CC ?= psp-gcc
PSP_AR ?= psp-ar
MKSFOEX ?= mksfoex
PACK_PBP ?= pack-pbp
PSP_FIXUP_IMPORTS ?= psp-fixup-imports
N64PSP_QUEUE_COUNTERS ?= 0
N64PSP_PSP_BENCHMARKS ?= 0

HOST_CC ?= cc
BUILD_PSP := build-psp
BUILD_HOST := build-host

PSPDEV ?= $(shell $(PSP_CONFIG) --pspdev-path)
PSPSDK ?= $(shell $(PSP_CONFIG) --pspsdk-path)

COMMON_SOURCES := \
	src/runtime/message_queue.c \
	src/runtime/result.c \
	src/runtime/runtime.c \
	src/bridge/bridge.c \
	src/renderer/trace_backend.c

PSP_PLATFORM_SOURCES := src/platform/psp/platform_psp.c
HOST_PLATFORM_SOURCES := src/platform/host/platform_host.c

PSP_RUNTIME_OBJECTS := $(patsubst %.c,$(BUILD_PSP)/%.o,$(filter src/runtime/%,$(COMMON_SOURCES)))
PSP_BRIDGE_OBJECTS := $(patsubst %.c,$(BUILD_PSP)/%.o,$(filter src/bridge/%,$(COMMON_SOURCES)))
PSP_TRACE_OBJECTS := $(patsubst %.c,$(BUILD_PSP)/%.o,$(filter src/renderer/%,$(COMMON_SOURCES)))
PSP_PLATFORM_OBJECTS := $(patsubst %.c,$(BUILD_PSP)/%.o,$(PSP_PLATFORM_SOURCES))
PSP_SMOKE_OBJECT := $(BUILD_PSP)/examples/psp_smoke/main.o

PSP_LIBS := \
	$(BUILD_PSP)/libn64psp_runtime.a \
	$(BUILD_PSP)/libn64psp_bridge.a \
	$(BUILD_PSP)/libn64psp_trace_backend.a \
	$(BUILD_PSP)/libn64psp_platform_psp.a

PSP_OPT_FLAGS ?= -O2
PSP_CFLAGS := -std=c99 -Wall -Wextra -Wconversion -Wshadow -Iinclude -I$(PSPDEV)/psp/include -I$(PSPSDK)/include \
	-D__PSP__ -DPSP -D_PSP_FW_VERSION=600
PSP_CFLAGS += $(PSP_OPT_FLAGS)
ifeq ($(N64PSP_QUEUE_COUNTERS),1)
PSP_CFLAGS += -DN64PSP_QUEUE_COUNTERS=1
endif
ifeq ($(N64PSP_PSP_BENCHMARKS),1)
PSP_CFLAGS += -DN64PSP_PSP_BENCHMARKS=1
endif
PSP_LDFLAGS := -L$(PSPDEV)/lib -L$(PSPDEV)/psp/lib -L$(PSPSDK)/lib -Wl,-zmax-page-size=128
PSP_LDLIBS := -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpsprtc -lpspkernel -lpspsdk -lc

HOST_SOURCES := $(COMMON_SOURCES) $(HOST_PLATFORM_SOURCES)
HOST_TEST_OBJECTS := $(patsubst %.c,$(BUILD_HOST)/%.o,$(HOST_SOURCES) tests/test_main.c)
HOST_SMOKE_OBJECTS := $(patsubst %.c,$(BUILD_HOST)/%.o,$(HOST_SOURCES) examples/host_smoke/main.c)
HOST_BENCHMARK_OBJECTS := $(patsubst %.c,$(BUILD_HOST)/%.o,$(HOST_SOURCES) examples/host_benchmark/main.c)
HOST_CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Iinclude
ifeq ($(N64PSP_QUEUE_COUNTERS),1)
HOST_CFLAGS += -DN64PSP_QUEUE_COUNTERS=1
endif
HOST_LDLIBS := -pthread

.PHONY: all psp eboot test smoke-host benchmark-host inspect-psp clean distclean

all: psp

psp: eboot

eboot: $(BUILD_PSP)/EBOOT.PBP

$(BUILD_PSP)/%.o: %.c
	@mkdir -p $(@D)
	$(PSP_CC) $(PSP_CFLAGS) -c $< -o $@

$(BUILD_PSP)/libn64psp_runtime.a: $(PSP_RUNTIME_OBJECTS)
	$(PSP_AR) rcs $@ $^

$(BUILD_PSP)/libn64psp_bridge.a: $(PSP_BRIDGE_OBJECTS)
	$(PSP_AR) rcs $@ $^

$(BUILD_PSP)/libn64psp_trace_backend.a: $(PSP_TRACE_OBJECTS)
	$(PSP_AR) rcs $@ $^

$(BUILD_PSP)/libn64psp_platform_psp.a: $(PSP_PLATFORM_OBJECTS)
	$(PSP_AR) rcs $@ $^

$(BUILD_PSP)/n64psp_psp_smoke: $(PSP_SMOKE_OBJECT) $(PSP_LIBS)
	$(PSP_CC) $(PSP_LDFLAGS) -o $@ $(PSP_SMOKE_OBJECT) \
		$(BUILD_PSP)/libn64psp_bridge.a \
		$(BUILD_PSP)/libn64psp_trace_backend.a \
		$(BUILD_PSP)/libn64psp_platform_psp.a \
		$(BUILD_PSP)/libn64psp_runtime.a \
		$(PSP_LDLIBS)
	$(PSP_FIXUP_IMPORTS) $@

$(BUILD_PSP)/PARAM.SFO: $(BUILD_PSP)/n64psp_psp_smoke
	$(MKSFOEX) -d MEMSIZE=1 -s APP_VER=01.00 "n64psp smoke" $@

$(BUILD_PSP)/EBOOT.PBP: $(BUILD_PSP)/n64psp_psp_smoke $(BUILD_PSP)/PARAM.SFO
	$(PACK_PBP) $@ $(BUILD_PSP)/PARAM.SFO NULL NULL NULL NULL NULL $(BUILD_PSP)/n64psp_psp_smoke NULL

$(BUILD_HOST)/%.o: %.c
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(BUILD_HOST)/n64psp_tests: $(HOST_TEST_OBJECTS)
	$(HOST_CC) -o $@ $^ $(HOST_LDLIBS)

$(BUILD_HOST)/n64psp_host_smoke: $(HOST_SMOKE_OBJECTS)
	$(HOST_CC) -o $@ $^ $(HOST_LDLIBS)

$(BUILD_HOST)/n64psp_host_benchmark: $(HOST_BENCHMARK_OBJECTS)
	$(HOST_CC) -o $@ $^ $(HOST_LDLIBS)

test: $(BUILD_HOST)/n64psp_tests
	./$(BUILD_HOST)/n64psp_tests

smoke-host: $(BUILD_HOST)/n64psp_host_smoke
	./$(BUILD_HOST)/n64psp_host_smoke

benchmark-host: $(BUILD_HOST)/n64psp_host_benchmark
	./$(BUILD_HOST)/n64psp_host_benchmark

inspect-psp: $(BUILD_PSP)/EBOOT.PBP
	file $(BUILD_PSP)/n64psp_psp_smoke $(BUILD_PSP)/EBOOT.PBP
	psp-size $(BUILD_PSP)/n64psp_psp_smoke
	psp-readelf -h $(BUILD_PSP)/n64psp_psp_smoke
	psp-objdump -f $(BUILD_PSP)/n64psp_psp_smoke
	wc -c $(BUILD_PSP)/n64psp_psp_smoke $(BUILD_PSP)/EBOOT.PBP

clean:
	$(RM) -r $(BUILD_PSP) $(BUILD_HOST)

distclean: clean
