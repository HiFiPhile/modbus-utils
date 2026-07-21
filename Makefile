OUTPUT_DIR := build
VERSION := $(strip $(shell sed -n '1p' VERSION))

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

CC ?= cc
PKG_CONFIG ?= pkg-config

DEBUG ?= 0
STATIC ?= 0
USE_SYSTEM_LIBMODBUS ?= 0

CPPFLAGS += -I. -Iargtable3 -DMBU_VERSION=\"$(VERSION)\"
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu99 -Wall -Wextra -Wpedantic

ifeq ($(DEBUG),1)
CFLAGS += -O0 -g3
endif

ARGTABLE_SOURCE := argtable3/argtable3.c
ARGTABLE_HEADER := argtable3/argtable3.h
COMMON_SOURCE := mbu-common.c
COMMON_HEADER := mbu-common.h

CLIENT_OBJECTS := $(OUTPUT_DIR)/modbusc.o $(OUTPUT_DIR)/mbu-common.o $(OUTPUT_DIR)/argtable3.o
SERVER_OBJECTS := $(OUTPUT_DIR)/modbuss.o $(OUTPUT_DIR)/mbu-common.o $(OUTPUT_DIR)/argtable3.o
OBJECTS := $(sort $(CLIENT_OBJECTS) $(SERVER_OBJECTS))
DEPS := $(OBJECTS:.o=.d)
PROGRAMS := $(OUTPUT_DIR)/modbusc $(OUTPUT_DIR)/modbuss
TEST_PROGRAM := $(OUTPUT_DIR)/test-common
FORMAT_SOURCES := mbu-common.c mbu-common.h modbusc.c modbuss.c tests/test-common.c
STRICT_WARNINGS := -Wconversion -Wshadow -Wformat=2 -Werror
SANITIZER_DIR := $(OUTPUT_DIR)/sanitize
SANITIZER_FLAGS := -O1 -g -std=gnu99 -Wall -Wextra -Wpedantic -Werror \
	-fsanitize=address,undefined -fno-omit-frame-pointer

ifeq ($(USE_SYSTEM_LIBMODBUS),1)
PKG_CONFIG_LIBS_FLAG := --libs
ifeq ($(STATIC),1)
PKG_CONFIG_LIBS_FLAG := --libs --static
endif
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags libmodbus)
LIBMODBUS_LIBS := $(shell $(PKG_CONFIG) $(PKG_CONFIG_LIBS_FLAG) libmodbus)
LIBMODBUS_DEPS :=
else
LIBMODBUS_CONFIG_STAMP := $(OUTPUT_DIR)/.libmodbus-configured
LIBMODBUS_LIBRARY := libmodbus/src/.libs/libmodbus.a
CPPFLAGS += -Ilibmodbus/src
LIBMODBUS_LIBS := $(LIBMODBUS_LIBRARY)
LIBMODBUS_DEPS := $(LIBMODBUS_LIBRARY)
endif

LDLIBS += $(LIBMODBUS_LIBS) -lm

ifneq (,$(findstring MINGW,$(MSYSTEM)))
LDLIBS += -lws2_32
endif

.PHONY: all check clean distclean format format-check install lib sanitize test uninstall

all: $(PROGRAMS)

$(OUTPUT_DIR)/modbusc: $(CLIENT_OBJECTS) $(LIBMODBUS_DEPS)
	$(CC) $(LDFLAGS) $(CLIENT_OBJECTS) $(LDLIBS) -o $@

$(OUTPUT_DIR)/modbuss: $(SERVER_OBJECTS) $(LIBMODBUS_DEPS)
	$(CC) $(LDFLAGS) $(SERVER_OBJECTS) $(LDLIBS) -o $@

$(OUTPUT_DIR)/modbusc.o: modbusc.c $(COMMON_HEADER) $(ARGTABLE_HEADER) VERSION | \
		$(OUTPUT_DIR) $(LIBMODBUS_DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTPUT_DIR)/modbuss.o: modbuss.c $(COMMON_HEADER) $(ARGTABLE_HEADER) VERSION | \
		$(OUTPUT_DIR) $(LIBMODBUS_DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTPUT_DIR)/argtable3.o: $(ARGTABLE_SOURCE) $(ARGTABLE_HEADER) | $(OUTPUT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTPUT_DIR)/mbu-common.o: $(COMMON_SOURCE) $(COMMON_HEADER) | $(OUTPUT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(TEST_PROGRAM): tests/test-common.c $(OUTPUT_DIR)/mbu-common.o | $(OUTPUT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(OUTPUT_DIR):
	mkdir -p $@

ifneq ($(USE_SYSTEM_LIBMODBUS),1)
$(LIBMODBUS_CONFIG_STAMP): libmodbus/configure | $(OUTPUT_DIR)
	cd libmodbus && CC="$(CC)" ./configure --enable-static --disable-shared $(CONF_OPT)
	$(MAKE) -C libmodbus clean
	touch $@

$(LIBMODBUS_LIBRARY): $(LIBMODBUS_CONFIG_STAMP)
	$(MAKE) -C libmodbus/src
endif

lib: $(LIBMODBUS_DEPS)

test: all $(TEST_PROGRAM)
	$(TEST_PROGRAM)
	sh tests/cli-tests.sh $(OUTPUT_DIR)/modbusc $(OUTPUT_DIR)/modbuss
	sh tests/tcp-loopback.sh $(OUTPUT_DIR)/modbusc $(OUTPUT_DIR)/modbuss

format:
	clang-format -i $(FORMAT_SOURCES)

format-check:
	clang-format --dry-run --Werror $(FORMAT_SOURCES)

check: format-check
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_WARNINGS) -fsyntax-only \
		$(COMMON_SOURCE) modbusc.c modbuss.c tests/test-common.c

$(SANITIZER_DIR):
	mkdir -p $@

$(SANITIZER_DIR)/modbusc: modbusc.c $(COMMON_SOURCE) $(COMMON_HEADER) \
		$(ARGTABLE_SOURCE) $(ARGTABLE_HEADER) VERSION $(LIBMODBUS_DEPS) | $(SANITIZER_DIR)
	$(CC) $(CPPFLAGS) $(SANITIZER_FLAGS) modbusc.c $(COMMON_SOURCE) $(ARGTABLE_SOURCE) \
		$(LIBMODBUS_LIBS) -lm -o $@

$(SANITIZER_DIR)/modbuss: modbuss.c $(COMMON_SOURCE) $(COMMON_HEADER) \
		$(ARGTABLE_SOURCE) $(ARGTABLE_HEADER) VERSION $(LIBMODBUS_DEPS) | $(SANITIZER_DIR)
	$(CC) $(CPPFLAGS) $(SANITIZER_FLAGS) modbuss.c $(COMMON_SOURCE) $(ARGTABLE_SOURCE) \
		$(LIBMODBUS_LIBS) -lm -o $@

$(SANITIZER_DIR)/test-common: tests/test-common.c $(COMMON_SOURCE) $(COMMON_HEADER) | $(SANITIZER_DIR)
	$(CC) $(CPPFLAGS) $(SANITIZER_FLAGS) tests/test-common.c $(COMMON_SOURCE) -o $@

sanitize: $(SANITIZER_DIR)/modbusc $(SANITIZER_DIR)/modbuss $(SANITIZER_DIR)/test-common
	$(SANITIZER_DIR)/test-common
	sh tests/cli-tests.sh $(SANITIZER_DIR)/modbusc $(SANITIZER_DIR)/modbuss
	sh tests/tcp-loopback.sh $(SANITIZER_DIR)/modbusc $(SANITIZER_DIR)/modbuss

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(PROGRAMS) $(DESTDIR)$(BINDIR)

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/modbusc $(DESTDIR)$(BINDIR)/modbuss

clean:
	$(RM) -r $(OUTPUT_DIR)

distclean: clean

-include $(DEPS)
