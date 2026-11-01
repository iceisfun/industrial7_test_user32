# Each directory under tests/ becomes build/<dir>.exe (all .c in it + common/t.c).
# user32 is deliberately NOT linked; everything resolves via GetProcAddress.
CC      = x86_64-w64-mingw32-gcc
SYSINC  ?= /usr/x86_64-w64-mingw32/include
SYSLIB  ?= /usr/x86_64-w64-mingw32/lib
CFLAGS  ?= -O1 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format
AREAS   := $(notdir $(wildcard tests/*))
EXES    := $(addprefix build/,$(addsuffix .exe,$(AREAS)))
all: $(EXES)
.SECONDEXPANSION:
build/%.exe: $$(wildcard tests/$$*/*.c) $$(wildcard tests/$$*/*.h) common/t.c common/t.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Icommon -o $@ $(wildcard tests/$*/*.c) common/t.c -static -lkernel32 -ladvapi32 -lgdi32 -lntdll
clean: ; rm -rf build
.PHONY: all clean
# Harness self-test (deliberate failures, crashes, hangs); not part of the corpus: build/selftest/selftest.exe
selftest: common/t.c common/t.h tools/selftest/selftest.c
	@mkdir -p build/selftest
	$(CC) $(CFLAGS) -Icommon -o build/selftest/selftest.exe tools/selftest/selftest.c common/t.c -static -lkernel32 -ladvapi32 -lgdi32 -lntdll
.PHONY: selftest
