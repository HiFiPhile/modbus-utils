OUTPUT_DIR = build

CFLAGS ?= -O2 -g -std=gnu99

DEBUG ?= 0
ifeq ($(DEBUG), 0)
    CFLAGS += -s
endif

STATIC ?= 0
ifeq ($(STATIC), 1)
    CFLAGS += -Wl,-Bstatic
endif

INCLUDES := -I./libmodbus/src \
			-I./argtable3 \
			-L./libmodbus/src/.libs \

SRC_CLIENT := modbusc.c

SRC_SERVER := modbuss.c

SRC_COMMMON := argtable3/argtable3.c

LIBS = $(LIB_MODBUS) \
		-lmodbus \
		-lm	\

ifeq ($(MSYSTEM),MINGW64)
	LIBS += -lws2_32
endif

all: $(OUTPUT_DIR)/modbusc $(OUTPUT_DIR)/modbuss

$(OUTPUT_DIR)/modbusc: $(SRC_CLIENT) | $(OUTPUT_DIR)/.out
	$(CC) $(CFLAGS) $(SRC_CLIENT) $(SRC_COMMMON) $(INCLUDES) $(LIBS) -Wl,-Bdynamic -o $@

$(OUTPUT_DIR)/modbuss: $(SRC_SERVER) $(LIB_MODBUS) | $(OUTPUT_DIR)/.out
	$(CC) $(CFLAGS) $(SRC_SERVER) $(SRC_COMMMON) $(INCLUDES) $(LIBS) -Wl,-Bdynamic -o $@

$(OUTPUT_DIR)/.out:
	mkdir -p $(OUTPUT_DIR)
	touch $(OUTPUT_DIR)/.out

lib:
	pushd libmodbus/ && ./configure --enable-static $(CONF_OPT)
	+$(MAKE) --directory=libmodbus/src/

.PHONY: all clean lib

clean:
	rm -rf $(OUTPUT_DIR)/
