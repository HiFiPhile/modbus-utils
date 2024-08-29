OUTPUT_DIR = build

CFLAGS := -O2 -s

INCLUDES := -I./libmodbus/src \
			-I./argtable3 \

LIBS := -L./libmodbus/src/.libs/ \
       -l:libmodbus.a \

SRC_CLIENT := modbus_client.c

SRC_SERVER := modbus_server.c

SRC_COMMMON := argtable3/argtable3.c

ifeq ($(MSYSTEM),MINGW64)
	LIBS += -lws2_32
endif

all: $(OUTPUT_DIR)/modbus_client $(OUTPUT_DIR)/modbus_server

$(OUTPUT_DIR)/modbus_client: $(SRC_CLIENT) | $(OUTPUT_DIR)/.out
	$(CC) $(CFLAGS) $(SRC_CLIENT) $(SRC_COMMMON) $(INCLUDES) $(LIBS) -o $(OUTPUT_DIR)/modbus_client

$(OUTPUT_DIR)/modbus_server: $(SRC_SERVER) | $(OUTPUT_DIR)/.out
	$(CC) $(CFLAGS) $(SRC_SERVER) $(SRC_COMMMON) $(INCLUDES) $(LIBS) -o $(OUTPUT_DIR)/modbus_server

$(OUTPUT_DIR)/.out:
	mkdir -p $(OUTPUT_DIR)
	touch $(OUTPUT_DIR)/.out

debug: CFLAGS += -g
debug: CFLAGS := $(filter-out -s, $(CFLAGS))
debug: all

.PHONY: all clean

clean:
	rm -rf $(OUTPUT_DIR)/
