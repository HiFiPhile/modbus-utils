OUTPUT_DIR = build

CFLAGS := -O2 -s

INCLUDES := -I./libmodbus/src \

LIBS := -L./libmodbus/src/.libs/ \
       -l:libmodbus.a \

ifeq ($(MSYSTEM),MINGW64)
	LIBS += -lws2_32
endif

all: modbus_client modbus_server

modbus_client: modbus_client.c mbu-common.h | dir
	$(CC) $(CFLAGS) modbus_client.c $(INCLUDES) $(LIBS) -o $(OUTPUT_DIR)/modbus_client

modbus_server: modbus_server.c mbu-common.h | dir
	$(CC) $(CFLAGS) modbus_server.c $(INCLUDES) $(LIBS) -o $(OUTPUT_DIR)/modbus_server

dir:
	mkdir -p $(OUTPUT_DIR)

debug: CFLAGS += -g
debug: CFLAGS := $(filter-out -s, $(CFLAGS))
debug: all

.PHONY: clean

clean: 
	rm -rf $(OUTPUT_DIR)/
    