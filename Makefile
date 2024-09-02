OUTPUT_DIR = build

CFLAGS := -O2 -s -std=gnu99

INCLUDES := -I./libmodbus/src \
			-I./argtable3 \

SRC_CLIENT := modbusc.c

SRC_SERVER := modbuss.c

SRC_COMMMON := argtable3/argtable3.c

LIB_MODBUS := ./libmodbus/src/.libs/libmodbus.a

LIBS = $(LIB_MODBUS) \
	   -lm

ifeq ($(MSYSTEM),MINGW64)
	LIBS += -lws2_32
endif

all: $(OUTPUT_DIR)/modbusc $(OUTPUT_DIR)/modbuss

$(OUTPUT_DIR)/modbusc: $(SRC_CLIENT) $(LIB_MODBUS) | $(OUTPUT_DIR)/.out
	$(CC) $(CFLAGS) $(SRC_CLIENT) $(SRC_COMMMON) $(INCLUDES) $(LIBS) -o $(OUTPUT_DIR)/modbusc

$(OUTPUT_DIR)/modbuss: $(SRC_SERVER) $(LIB_MODBUS) | $(OUTPUT_DIR)/.out
	$(CC) $(CFLAGS) $(SRC_SERVER) $(SRC_COMMMON) $(INCLUDES) $(LIBS) -o $(OUTPUT_DIR)/modbuss

$(OUTPUT_DIR)/.out:
	mkdir -p $(OUTPUT_DIR)
	touch $(OUTPUT_DIR)/.out

$(LIB_MODBUS):
	pushd libmodbus/ && ./configure --enable-static $(CONF_OPT)
	+$(MAKE) --directory=libmodbus/src/

debug: CFLAGS += -g
debug: CFLAGS := $(filter-out -s, $(CFLAGS))
debug: all

.PHONY: all clean lib

clean:
	rm -rf $(OUTPUT_DIR)/
	+$(MAKE) --directory=libmodbus/ clean
