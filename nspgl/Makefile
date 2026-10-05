# nspGL - WebGL + HTML viewer for TI-Nspire CX (Ndless)
#   make            -> nspgl.tns (needs Ndless SDK in PATH)
#   make host       -> build/nspgl-host (headless desktop test harness)
QJS_VER := $(shell cat src/js/qjs/VERSION)
SRC_APP := src/app/browser.c src/app/page.c src/app/layout.c src/app/html.c src/app/gfx.c src/app/css.c
SRC_GL  := src/gl/sgl.c src/gl/glsl_gen.c src/gl/glsl_parse.c src/gl/glsl_pp.c src/gl/glsl_run.c
SRC_JS  := src/js/webgl_js.c src/js/canvas2d.c
SRC_QJS := src/js/qjs/quickjs.c src/js/qjs/libregexp.c src/js/qjs/libunicode.c src/js/qjs/cutils.c src/js/qjs/dtoa.c
SRCS := $(SRC_APP) $(SRC_GL) $(SRC_JS) $(SRC_QJS)
STB ?= /usr/include/stb
GEN := src/app/prelude.inc src/app/fontdata.h src/js/webgl_consts.inc

DEFS := -DCONFIG_VERSION=\"$(QJS_VER)\" -D_GNU_SOURCE -I$(STB) -Isrc/app

# ---------- calculator ----------
NCC := nspire-gcc
NOPT := -O2 -marm -mcpu=arm926ej-s -ffunction-sections -fdata-sections -fno-strict-aliasing -Wall -Wno-unused-parameter -Wno-sign-compare \
        -include src/platform/nspire_compat.h
NOBJ := $(patsubst src/%.c,build/nspire/%.o,$(SRCS) src/platform/platform_nspire.c)

all: nspgl.tns

build/nspire/%.o: src/%.c $(GEN)
	@mkdir -p $(dir $@)
	@echo "  CC  $<"
	@$(NCC) $(NOPT) $(DEFS) -c $< -o $@

build/nspgl.elf: $(NOBJ)
	nspire-ld $(NOBJ) -o $@ -lm -Wl,--gc-sections

nspgl.tns: build/nspgl.elf
	genzehn --input $< --output build/nspgl.zehn --name "nspGL" --version 10 --author "nspGL" --notice "WebGL + HTML viewer" \
	        --compress --uses-lcd-blit true --240x320-support true
	make-prg build/nspgl.zehn $@
	@ls -la $@

# ---------- host test harness ----------
HCC := gcc
HOPT ?= -O2 -g -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-misleading-indentation
HOBJ := $(patsubst src/%.c,build/host/%.o,$(SRCS) src/platform/platform_host.c)

host: build/nspgl-host

build/host/%.o: src/%.c $(GEN)
	@mkdir -p $(dir $@)
	@echo "  HOSTCC  $<"
	@$(HCC) $(HOPT) $(DEFS) -c $< -o $@

build/nspgl-host: $(HOBJ)
	$(HCC) $(HOPT) $(HOBJ) -o $@ -lm $(HLIBS)

# ---------- generated ----------
src/app/prelude.inc: src/js/prelude.js tools/embed.py
	python3 tools/embed.py $< $@
src/app/fontdata.h: tools/genfont.py
	python3 tools/genfont.py $@
src/js/webgl_consts.inc: tools/genconsts.py
	python3 tools/genconsts.py

clean:
	rm -rf build nspgl.tns

.PHONY: all host clean
