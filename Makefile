# Makefile
#
# Copyright (C) 2025, Charles Chiou

MAKEFLAGS =	--no-print-dir

PICO_SDK_PATH :=	$(realpath pico-sdk)
export PICO_SDK_PATH

TARGETS +=	build/meshroom.uf2

.PHONY: default clean distclean $(TARGETS)

default: $(TARGETS)

clean:
	@test -f build/Makefile && $(MAKE) -C build clean

distclean:
	rm -rf build/

.PHONY: meshroom

meshroom: build/meshroom.uf2

MESHROOM_TREE :=	\
	CMakeLists.txt version.h.in \
	$(wildcard *.cxx) $(wildcard *.hxx) $(wildcard *.h) \
	libmeshtastic pico-plat

build/meshroom.uf2: build/Makefile
	@if [ -f $@ ] && [ -n "`find -H $(MESHROOM_TREE) -type f \
	    \( -name '*.c' -o -name '*.cxx' -o -name '*.h' -o -name '*.hxx' \
	       -o -name 'CMakeLists.txt' -o -name 'version.h.in' \) \
	    -newer $@ -print -quit`" ]; then \
		rm -f build/version.h; \
	fi
	@$(MAKE) -C build

build/Makefile: CMakeLists.txt
	@mkdir -p build
	@cd build && cmake -DPICO_BOARD=pico_w ..

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
	@openocd -f /usr/share/openocd/scripts/interface/cmsis-dap.cfg -f /usr/share/openocd/scripts/target/rp2040.cfg -c "adapter speed 5000; init; halt; reset; exit"

.PHONY: gdb

gdb: build/meshroom.elf
	@gdb-multiarch $< -ex 'target remote localhost:3333'

# Firmware flashing on Linux host
.PHONY: flash
flash: build/meshroom.uf2
	@sync && cp build/meshroom.uf2 /mnt/pico && sync
