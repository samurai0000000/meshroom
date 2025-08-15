# Makefile
#
# Copyright (C) 2025, Charles Chiou

MAKEFLAGS =	--no-print-dir

PICO_SDK_PATH :=	$(realpath pico-sdk)
export PICO_SDK_PATH

FREERTOS_KERNEL_PATH :=	$(realpath FreeRTOS-Kernel)
export FREERTOS_KERNEL_PATH

TARGETS +=	build/meshroom.uf2

.PHONY: default clean distclean $(TARGETS)

default: $(TARGETS)

clean:
	@test -f build/Makefile && $(MAKE) -C build clean

distclean:
	rm -rf build/

.PHONY: meshroom

meshroom: build/meshroom.uf2

build/meshroom.uf2: build/Makefile
	@$(MAKE) -C build

build/Makefile: CMakeLists.txt
	@mkdir -p build
	@cd build && cmake ..

.PHONY: release

release: build/Makefile
	@rm -f build/version.h
	@$(MAKE) -C build

# Development & debug targets

.PHONY: openocd

openocd:
	@openocd -f /usr/share/openocd/scripts/interface/cmsis-dap.cfg -f /usr/share/openocd/scripts/target/rp2040.cfg -c "adapter speed 5000"

.PHONY: openocd-reset

openocd-reset:
	@openocd -f /usr/share/openocd/scripts/interface/cmsis-dap.cfg -f /usr/share/openocd/scripts/target/rp2040.cfg -c "init; reset; exit"

.PHONY: gdb

gdb: build/meshroom.elf
	@gdb-multiarch $< -ex 'target remote localhost:3333'
