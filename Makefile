PSP_CONFIG ?= psp-config
PSP_CC ?= psp-gcc
PSP_AR ?= psp-ar
MKSFOEX ?= mksfoex
PACK_PBP ?= pack-pbp
PSP_FIXUP_IMPORTS ?= psp-fixup-imports

HOST_CC ?= cc
HOST_AR ?= ar

N64PSP_QUEUE_COUNTERS ?= 0
N64PSP_PSP_BENCHMARKS ?= 0
N64PSP_USE_VFPU ?= 1
N64PSP_VFPU_TRANSFORM_EXPERIMENT ?= 0

BUILD_PSP ?= build-psp
BUILD_HOST := build-host
ARTIFACTS_DIR ?= artifacts
N64PSP_GIT_COMMIT ?= $(shell git rev-parse --short=12 HEAD 2>/dev/null || printf unknown)
PSP_COMPILE_FLAGS_STAMP := $(BUILD_PSP)/.compile-flags

PSPDEV ?= $(shell $(PSP_CONFIG) --pspdev-path)
PSPSDK ?= $(shell $(PSP_CONFIG) --pspsdk-path)

COMMON_SOURCES := \
	src/runtime/message_queue.c \
	src/runtime/result.c \
	src/runtime/runtime.c \
	src/bridge/bridge.c \
	src/renderer/trace_backend.c

MATH_SOURCES := \
	src/math/math_api.c \
	src/math/math_scalar.c

PSP_PLATFORM_SOURCES := \
	src/platform/psp/platform_psp.c

HOST_PLATFORM_SOURCES := \
	src/platform/host/platform_host.c

PSP_MATH_ASM_SOURCES :=

ifeq ($(N64PSP_USE_VFPU),1)
PSP_MATH_ASM_SOURCES += \
	src/platform/psp/math_vfpu.S
endif

PSP_RUNTIME_OBJECTS := \
	$(patsubst %.c,$(BUILD_PSP)/%.o,$(filter src/runtime/%,$(COMMON_SOURCES)))

PSP_BRIDGE_OBJECTS := \
	$(patsubst %.c,$(BUILD_PSP)/%.o,$(filter src/bridge/%,$(COMMON_SOURCES)))

PSP_TRACE_OBJECTS := \
	$(patsubst %.c,$(BUILD_PSP)/%.o,$(filter src/renderer/%,$(COMMON_SOURCES)))

PSP_PLATFORM_OBJECTS := \
	$(patsubst %.c,$(BUILD_PSP)/%.o,$(PSP_PLATFORM_SOURCES))

PSP_MATH_C_OBJECTS := \
	$(patsubst %.c,$(BUILD_PSP)/%.o,$(MATH_SOURCES))

PSP_MATH_ASM_OBJECTS := \
	$(patsubst %.S,$(BUILD_PSP)/%.o,$(PSP_MATH_ASM_SOURCES))

PSP_MATH_OBJECTS := \
	$(PSP_MATH_C_OBJECTS) \
	$(PSP_MATH_ASM_OBJECTS)

PSP_SMOKE_SOURCES := \
	examples/psp_smoke/main.c \
	examples/psp_smoke/math_smoke.c

PSP_SMOKE_OBJECTS := \
	$(patsubst %.c,$(BUILD_PSP)/%.o,$(PSP_SMOKE_SOURCES))

PSP_LIBS := \
	$(BUILD_PSP)/libn64psp_runtime.a \
	$(BUILD_PSP)/libn64psp_bridge.a \
	$(BUILD_PSP)/libn64psp_trace_backend.a \
	$(BUILD_PSP)/libn64psp_platform_psp.a \
	$(BUILD_PSP)/libn64psp_math.a

PSP_OPT_FLAGS ?= -O2

PSP_CFLAGS := \
	-std=c99 \
	-Wall \
	-Wextra \
	-Wconversion \
	-Wshadow \
	-Iinclude \
	-I$(PSPDEV)/psp/include \
	-I$(PSPSDK)/include \
	-D__PSP__ \
	-DPSP \
	-D_PSP_FW_VERSION=600 \
	-DN64PSP_USE_VFPU=$(N64PSP_USE_VFPU) \
	-DN64PSP_VFPU_TRANSFORM_EXPERIMENT=$(N64PSP_VFPU_TRANSFORM_EXPERIMENT) \
	-DN64PSP_GIT_COMMIT=\"$(N64PSP_GIT_COMMIT)\" \
	-DN64PSP_PSP_OPT_FLAGS=\"$(PSP_OPT_FLAGS)\"

PSP_CFLAGS += $(PSP_OPT_FLAGS)

ifeq ($(N64PSP_QUEUE_COUNTERS),1)
PSP_CFLAGS += -DN64PSP_QUEUE_COUNTERS=1
endif

ifeq ($(N64PSP_PSP_BENCHMARKS),1)
PSP_CFLAGS += -DN64PSP_PSP_BENCHMARKS=1
endif

PSP_LDFLAGS := \
	-L$(PSPDEV)/lib \
	-L$(PSPDEV)/psp/lib \
	-L$(PSPSDK)/lib \
	-Wl,-zmax-page-size=128

PSP_LDLIBS := \
	-lpspdebug \
	-lpspdisplay \
	-lpspge \
	-lpspctrl \
	-lpsprtc \
	-lpspsdk \
	-lc \
	-lpspuser

HOST_SOURCES := \
	$(COMMON_SOURCES) \
	$(HOST_PLATFORM_SOURCES)

HOST_MATH_OBJECTS := \
	$(patsubst %.c,$(BUILD_HOST)/%.o,$(MATH_SOURCES))

HOST_TEST_OBJECTS := \
	$(patsubst %.c,$(BUILD_HOST)/%.o,$(HOST_SOURCES) tests/test_main.c)

HOST_MATH_TEST_OBJECT := \
	$(BUILD_HOST)/tests/test_math.o

HOST_SMOKE_OBJECTS := \
	$(patsubst %.c,$(BUILD_HOST)/%.o,$(HOST_SOURCES) examples/host_smoke/main.c)

HOST_BENCHMARK_OBJECTS := \
	$(patsubst %.c,$(BUILD_HOST)/%.o,$(HOST_SOURCES) examples/host_benchmark/main.c)

HOST_CFLAGS := \
	-std=c99 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wconversion \
	-Wshadow \
	-Iinclude \
	-DN64PSP_USE_VFPU=0

ifeq ($(N64PSP_QUEUE_COUNTERS),1)
HOST_CFLAGS += -DN64PSP_QUEUE_COUNTERS=1
endif

HOST_LDLIBS := -pthread

.PHONY: \
	all \
	psp \
	eboot \
	test \
	test-runtime \
	test-math \
	smoke-host \
	benchmark-host \
	inspect-psp \
	psp-vfpu-ab \
	clean \
	distclean \
	FORCE

all: psp

psp: eboot

eboot: $(BUILD_PSP)/EBOOT.PBP

$(PSP_COMPILE_FLAGS_STAMP): FORCE
	@mkdir -p $(@D)
	@printf '%s\n' '$(PSP_CFLAGS)' > $@.tmp
	@if ! cmp -s $@.tmp $@; then mv $@.tmp $@; else rm -f $@.tmp; fi

$(BUILD_PSP)/%.o: %.c $(PSP_COMPILE_FLAGS_STAMP)
	@mkdir -p $(@D)
	$(PSP_CC) $(PSP_CFLAGS) -c $< -o $@

$(BUILD_PSP)/%.o: %.S $(PSP_COMPILE_FLAGS_STAMP)
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

$(BUILD_PSP)/libn64psp_math.a: $(PSP_MATH_OBJECTS)
	$(RM) $@
	$(PSP_AR) rcs $@ $^

$(BUILD_PSP)/n64psp_psp_smoke: $(PSP_SMOKE_OBJECTS) $(PSP_LIBS)
	$(PSP_CC) $(PSP_LDFLAGS) -o $@ $(PSP_SMOKE_OBJECTS) \
		$(BUILD_PSP)/libn64psp_bridge.a \
		$(BUILD_PSP)/libn64psp_trace_backend.a \
		$(BUILD_PSP)/libn64psp_math.a \
		$(BUILD_PSP)/libn64psp_platform_psp.a \
		$(BUILD_PSP)/libn64psp_runtime.a \
		$(PSP_LDLIBS)
	$(PSP_FIXUP_IMPORTS) $@

$(BUILD_PSP)/PARAM.SFO: $(BUILD_PSP)/n64psp_psp_smoke
	$(MKSFOEX) -d MEMSIZE=1 -s APP_VER=01.00 "n64psp smoke" $@

$(BUILD_PSP)/EBOOT.PBP: \
	$(BUILD_PSP)/n64psp_psp_smoke \
	$(BUILD_PSP)/PARAM.SFO
	$(PACK_PBP) $@ \
		$(BUILD_PSP)/PARAM.SFO \
		NULL \
		NULL \
		NULL \
		NULL \
		NULL \
		$(BUILD_PSP)/n64psp_psp_smoke \
		NULL

$(BUILD_HOST)/%.o: %.c
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

$(BUILD_HOST)/libn64psp_math.a: $(HOST_MATH_OBJECTS)
	$(RM) $@
	$(HOST_AR) rcs $@ $^

$(BUILD_HOST)/n64psp_tests: $(HOST_TEST_OBJECTS)
	$(HOST_CC) -o $@ $^ $(HOST_LDLIBS)

$(BUILD_HOST)/n64psp_math_tests: \
	$(HOST_MATH_TEST_OBJECT) \
	$(BUILD_HOST)/libn64psp_math.a
	$(HOST_CC) -o $@ \
		$(HOST_MATH_TEST_OBJECT) \
		$(BUILD_HOST)/libn64psp_math.a

$(BUILD_HOST)/n64psp_host_smoke: $(HOST_SMOKE_OBJECTS)
	$(HOST_CC) -o $@ $^ $(HOST_LDLIBS)

$(BUILD_HOST)/n64psp_host_benchmark: $(HOST_BENCHMARK_OBJECTS)
	$(HOST_CC) -o $@ $^ $(HOST_LDLIBS)

test-runtime: $(BUILD_HOST)/n64psp_tests
	./$(BUILD_HOST)/n64psp_tests

test-math: $(BUILD_HOST)/n64psp_math_tests
	./$(BUILD_HOST)/n64psp_math_tests

test: test-runtime test-math

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

psp-vfpu-ab:
	$(MAKE) clean BUILD_PSP=build-psp-vfpu-baseline
	$(MAKE) psp BUILD_PSP=build-psp-vfpu-baseline N64PSP_USE_VFPU=1 N64PSP_VFPU_TRANSFORM_EXPERIMENT=0 N64PSP_PSP_BENCHMARKS=1
	$(MAKE) clean BUILD_PSP=build-psp-vfpu-experiment
	$(MAKE) psp BUILD_PSP=build-psp-vfpu-experiment N64PSP_USE_VFPU=1 N64PSP_VFPU_TRANSFORM_EXPERIMENT=1 N64PSP_PSP_BENCHMARKS=1
	mkdir -p $(ARTIFACTS_DIR)/n64psp-vfpu-baseline $(ARTIFACTS_DIR)/n64psp-vfpu-experiment
	cp build-psp-vfpu-baseline/EBOOT.PBP $(ARTIFACTS_DIR)/n64psp-vfpu-baseline/EBOOT.PBP
	cp build-psp-vfpu-baseline/n64psp_psp_smoke $(ARTIFACTS_DIR)/n64psp-vfpu-baseline/n64psp_psp_smoke.elf
	cp build-psp-vfpu-experiment/EBOOT.PBP $(ARTIFACTS_DIR)/n64psp-vfpu-experiment/EBOOT.PBP
	cp build-psp-vfpu-experiment/n64psp_psp_smoke $(ARTIFACTS_DIR)/n64psp-vfpu-experiment/n64psp_psp_smoke.elf
	printf '%s\n' \
		'make psp BUILD_PSP=build-psp-vfpu-baseline N64PSP_USE_VFPU=1 N64PSP_VFPU_TRANSFORM_EXPERIMENT=0 N64PSP_PSP_BENCHMARKS=1' \
		'make psp BUILD_PSP=build-psp-vfpu-experiment N64PSP_USE_VFPU=1 N64PSP_VFPU_TRANSFORM_EXPERIMENT=1 N64PSP_PSP_BENCHMARKS=1' \
		> $(ARTIFACTS_DIR)/BUILD_COMMANDS.txt
	printf '%s\n' \
		'Install either artifact directory as ms0:/PSP/GAME/n64psp-vfpu-baseline or ms0:/PSP/GAME/n64psp-vfpu-experiment.' \
		'Run on real PSP hardware, not PPSSPP for timing proof.' \
		'Record the on-screen transform benchmark table for counts 1,2,3,4,8,16,31,32,63,64.' \
		'Compare chain2 VFPU, independent VFPU, and precompose + independent VFPU rows plus metadata printed by the EBOOT.' \
		> $(ARTIFACTS_DIR)/HARDWARE_TEST_PROTOCOL.txt
	( cd $(ARTIFACTS_DIR) && sha256sum n64psp-vfpu-baseline/EBOOT.PBP n64psp-vfpu-baseline/n64psp_psp_smoke.elf n64psp-vfpu-experiment/EBOOT.PBP n64psp-vfpu-experiment/n64psp_psp_smoke.elf > SHA256SUMS )

clean:
	$(RM) -r $(BUILD_PSP) $(BUILD_HOST)

distclean: clean
