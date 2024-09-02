/*
*  MIT License

*  Copyright (c) 2013  Krzysztow
*  Copyright (c) 2024  Zixun LI

*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:

*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <argtable3.h>

#include <modbus.h>
#include "errno.h"

#include "mbu-common.h"

#define PROGMANE "modbusc"

typedef enum {
    FuncNone =          -1,
    ReadCoils           = 0x01,
    ReadDiscreteInput   = 0x02,
    ReadHoldingRegisters= 0x03,
    ReadInputRegisters  = 0x04,
    WriteSingleCoil     = 0x05,
    WriteSingleRegister = 0x06,
    WriteMultipleCoils  = 0x0f,
    WriteMultipleRegisters  = 0x10
} FuncType;

typedef enum {
        DataInt,
        Data8Array,
        Data16Array
} WriteDataType;

typedef union {
    int dataInt;
    uint8_t *data8;
    uint16_t *data16;
} Data;

int process_request(modbus_t* ctx, int addrStart, int addrEnd, int func, int reg, int nb, WriteDataType dataType, Data data, const char* prefixScan);

int verbose = 0;

int main(int argc, char **argv)
{
    int c;
    int ok;
    int readWriteNo = 1;
    int isWriteFunction = 0;
    modbus_t *ctx;

    WriteDataType wDataType = DataInt;

    Data data;

    struct arg_rex *addr   = arg_rex1("a", "addr",       "^\\d{1,3}$|^\\d{1,3}\\.\\d{1,3}$",
                                                                    "<n>|<n.n>",        0,              "Slave address");
    struct arg_rem *addr1  = arg_rem("",                                                                "Use <n.n> for address scan");
    struct arg_int *reg    = arg_int1("r", "reg",                   "<n>",                              "Start register");
    struct arg_int *func   = arg_int1("f", "func",                  "<n>",                              "Modbus Function");
    struct arg_rem *func1  = arg_rem("",                                                                "    0x01 : Read Coils");
    struct arg_rem *func2  = arg_rem("",                                                                "    0x02 : Read Discrete Inputs");
    struct arg_rem *func3  = arg_rem("",                                                                "    0x03 : Read Holding Registers");
    struct arg_rem *func4  = arg_rem("",                                                                "    0x04 : Read Input Registers");
    struct arg_rem *func5  = arg_rem("",                                                                "    0x05 : Write Single Coil");
    struct arg_rem *func6  = arg_rem("",                                                                "    0x06 : Write Single Register");
    struct arg_rem *func7  = arg_rem("",                                                                "    0x0F : Write Multiple Coils");
    struct arg_rem *func8  = arg_rem("",                                                                "    0x10 : Write Multiple registers");
    struct arg_int *dwrite = arg_intn("w", "write",                 "<n>", 0, 123,                      "Data to write");
    struct arg_int *count  = arg_int0("c", "count",                 "<reg>",                            "Data read count");
    struct arg_int *tout   = arg_int0("o", "timeout",               "<ms>",                             "Request timeout");
    struct arg_lit *base1  = arg_lit0("1", "base-1",                                                    "Base 1 addressing");
    struct arg_lit *debug  = arg_litn("v", "verbose",                      0, 2,                        "Enable verbpse output");
    struct arg_lit *help   = arg_lit0("h", "help",                                                      "Print this help and exit");
    /* RTU */
    struct arg_rex *rtu    = arg_rex1(NULL, NULL,        "rtu",   NULL,               ARG_REX_ICASE,  NULL);
    struct arg_str *dev    = arg_str1("d", "dev",                   "<device>",                         "Serial device");
    struct arg_int *baud   = arg_intn("b", "baud",                  "<n>", 1, 16,                       "Baud rate");
    struct arg_rex *dbit   = arg_rex0(NULL, "data-bits", "^7$|^8$", "<7|8>=8",          ARG_REX_ICASE,  "Data bits");
    struct arg_rex *sbit   = arg_rex0(NULL, "stop-bits", "^1$|^2$", "<1|2>=1",          ARG_REX_ICASE,  "Stop bits");
    struct arg_rex *parity = arg_rexn("p", "parity",     "^N$|^E$|^O$",
                                                                    "<N|E|O>=E", 0, 3,  ARG_REX_ICASE,  "Parity");
    struct arg_end *end1    = arg_end(20);
    /* TCP */
    struct arg_rex *tcp    = arg_rex1(NULL, NULL,        "tcp",   NULL,               ARG_REX_ICASE,  NULL);
    struct arg_int *port   = arg_int0("p", "port",                  "<port>=502",                       "Socket listening port");
    struct arg_rex *ip     = arg_rex0("i", "addr", "^([0-9]{1,3}\\.){3}([0-9]{1,3})$",
                                                                    "<IP>=127.0.0.1",   ARG_REX_ICASE,  "Device IP address");
    struct arg_end *end2    = arg_end(20);

    void* argtable1[] = {rtu, addr, addr1, reg, func, func1, func2, func3, func4, func5, func6, func7, func8,
                            dev, baud, dbit, sbit, parity, dwrite, count, tout, base1, debug, help, end1};

    void* argtable2[] = {tcp, addr, addr1, reg, func, func1, func2, func3, func4, func5, func6, func7, func8,
                            port, ip, dwrite, count, tout, base1, debug, help, end2};

    /* defaults */
    count->ival[0]      = 1;
    tout->ival[0]       = 1000;
    dbit->sval[0]       = "8";
    sbit->sval[0]       = "1";
    port->ival[0]       = 502;
    ip->sval[0]         = "127.0.0.1";

    int nerrors1 = arg_parse(argc,argv,argtable1);
    int nerrors2 = arg_parse(argc,argv,argtable2);

    /* array defaults */
    if(parity->count == 0) {
        parity->count = 1;
        parity->sval[0] = "E";
    }

    /* special case: '--help' takes precedence over error reporting */
    if (help->count) {
        printf("Modbus client utils.\n\n");
        if (rtu->count) {
            arg_print_syntax(stdout, argtable1, "\n");
            arg_print_glossary(stdout, argtable1, "  %-30s %s\n");
        } else if (tcp->count) {
            arg_print_syntax(stdout, argtable2, "\n");
            arg_print_glossary(stdout, argtable2, "  %-30s %s\n");
        } else {
            printf("Missing <rtu|tcp> command.\n");
        }
        return 0;
    }
    /* If the parser returned any errors then display them and exit */
    if (rtu->count) {
        if(nerrors1) {
            /* Display the error details contained in the arg_end struct.*/
            arg_print_errors(stdout, end1, PROGMANE" rtu");
            printf("Try '%s --help' for more information.\n", PROGMANE" rtu");
            exit(EXIT_FAILURE);
        }
    } else if (tcp->count) {
        if(nerrors2) {
            /* Display the error details contained in the arg_end struct.*/
            arg_print_errors(stdout, end2, PROGMANE" tcp");
            printf("Try '%s --help' for more information.\n", PROGMANE" tcp");
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Missing <rtu|tcp> command.\n");
        printf("usage 1: %s ", PROGMANE);  arg_print_syntax(stdout,argtable1,"\n");
        printf("usage 2: %s ", PROGMANE);  arg_print_syntax(stdout,argtable2,"\n");
        exit(EXIT_FAILURE);
    }

    verbose = debug->count;

    bool addrScan = false;
    int addrStart;
    int addrEnd;
    if(sscanf(addr->sval[0], "%d.%d", &addrStart, &addrEnd) == 2) {
        addrScan = true;
    } else {
        addrEnd = addrStart;
    }

    if(base1->count) {
        reg->ival[0]--;
    }

    if(addrScan && addrStart >= addrEnd) {
        printf("%s:Scan ending address must be bigger than starting address.\n", PROGMANE);
        exit(EXIT_FAILURE);
    }

    //choose write data type
    switch (func->ival[0]) {
    case(ReadCoils):
        wDataType = Data8Array;
        break;
    case(ReadDiscreteInput):
        wDataType = DataInt;
        break;
    case(ReadHoldingRegisters):
    case(ReadInputRegisters):
        wDataType = Data16Array;
        break;
    case(WriteSingleCoil):
    case(WriteSingleRegister):
        wDataType = DataInt;
        isWriteFunction = 1;
        break;
    case(WriteMultipleCoils):
        wDataType = Data8Array;
        isWriteFunction = 1;
        break;
    case(WriteMultipleRegisters):
        wDataType = Data16Array;
        isWriteFunction = 1;
        break;
    default:
        printf("No correct function chosen");
        exit(EXIT_FAILURE);
    }

    if (isWriteFunction) {
        if(wDataType != DataInt)
            readWriteNo = dwrite->count;
        else
            readWriteNo = 1;
    } else {
        readWriteNo = count->ival[0];
    }

    //allocate buffer for data
    switch (wDataType) {
    case (DataInt):
        //no need to alloc anything
        break;
    case (Data8Array):
        data.data8 = malloc(readWriteNo * sizeof(uint8_t));
        break;
    case (Data16Array):
        data.data16 = malloc(readWriteNo * sizeof(uint16_t));
        break;
    default:
        printf("Data alloc error!\n");
        exit(EXIT_FAILURE);
    }

    if (isWriteFunction) {
        if (verbose)
            printf("Data to write: ");
        for (int i = 0; i < dwrite->count; i++) {
            if (wDataType == DataInt) {
                data.dataInt = dwrite->ival[0];
                if (verbose)
                    printf("0x%x", data.dataInt);
                /* only one data */
                break;
            } else if (wDataType == Data8Array) {
                data.data8[i] = dwrite->ival[i];
                if (verbose)
                    printf("0x%02x ", data.data8[i]);
            } if (wDataType == Data16Array) {
                data.data16[i] = dwrite->ival[i];
                if (verbose)
                    printf("0x%04x ", data.data16[i]);
            }
        }
        if (verbose)
            printf("\n");
    }

    if (rtu->count) {
        char prefix[32] = {0};
        int prefixLen = 0;
        bool scanMode = baud->count > 1 || parity->count > 1;

        for (int i = 0; i < baud->count; i++) {
            if(scanMode) {
                sprintf(prefix, "Baudrate:%d ", baud->ival[i]);
                prefixLen = strlen(prefix);
            }
            for (int j = 0; j < parity->count; j++) {
                if(scanMode)
                    sprintf(prefix + prefixLen, "Parity:%c ", toupper(parity->sval[j][0]));
                ctx = modbus_new_rtu(dev->sval[0],
                        baud->ival[i], toupper(parity->sval[j][0]), getInt(dbit->sval[0], 0), getInt(sbit->sval[0], 0));
                modbus_set_debug(ctx, verbose > 1);
                modbus_set_response_timeout(ctx, 0, tout->ival[0] * 1000);

                //issue the request
                int ret = -1;
                if (modbus_connect(ctx)) {
                    fprintf(stderr, "Connection failed: %s\n",
                            modbus_strerror(errno));
                    modbus_free(ctx);
                    return -1;
                }

                process_request(ctx, addrStart, addrEnd, func->ival[0], reg->ival[0], readWriteNo, wDataType, data, prefix);

                //cleanup
                modbus_close(ctx);
                modbus_free(ctx);
            }
        }
    } else {
        ctx = modbus_new_tcp(ip->sval[0], port->ival[0]);
        modbus_set_debug(ctx, verbose);
        modbus_set_response_timeout(ctx, 0, tout->ival[0] * 1000);

        //issue the request
        int ret = -1;
        if (modbus_connect(ctx)) {
            fprintf(stderr, "Connection failed: %s\n",
                    modbus_strerror(errno));
            modbus_free(ctx);
            return -1;
        }

        process_request(ctx, addrStart, addrEnd, func->ival[0], reg->ival[0], readWriteNo, wDataType, data, "");

        //cleanup
        modbus_close(ctx);
        modbus_free(ctx);
    }

    switch (wDataType) {
    case (DataInt):
        //nothing to be done
        break;
    case (Data8Array):
        free(data.data8);
        break;
    case (Data16Array):
        free(data.data16);
        break;
    }

    exit(0);
}

int process_request(modbus_t* ctx, int addrStart, int addrEnd, int func, int reg, int nb, WriteDataType dataType, Data data, const char* prefixScan)
{
    int ret = -1;
    bool isWriteFunction = false;
    bool addrScan = addrEnd != addrStart;
    for (int i = addrStart; i <= addrEnd; i++) {
        modbus_set_slave(ctx, i);

        switch (func) {
        case(ReadCoils):
            ret = modbus_read_bits(ctx, reg, nb, data.data8);
            break;
        case(ReadDiscreteInput):
            printf("ReadDiscreteInput: not implemented yet!\n");
            dataType = DataInt;
            break;
        case(ReadHoldingRegisters):
            ret = modbus_read_registers(ctx, reg, nb, data.data16);
            break;
        case(ReadInputRegisters):
            ret = modbus_read_input_registers(ctx, reg, nb, data.data16);
            break;
        case(WriteSingleCoil):
            ret = modbus_write_bit(ctx, reg, data.dataInt);
            isWriteFunction = true;
            break;
        case(WriteSingleRegister):
            ret = modbus_write_register(ctx, reg, data.dataInt);
            isWriteFunction = true;
            break;
        case(WriteMultipleCoils):
            ret = modbus_write_bits(ctx, reg, nb, data.data8);
            isWriteFunction = true;
            break;
        case(WriteMultipleRegisters):
            ret = modbus_write_registers(ctx, reg, nb, data.data16);
            isWriteFunction = true;
            break;
        default:
            break;
        }

        if (addrScan && (verbose || ret == nb))
            printf("%sAddress:%d\n", prefixScan, i);
        if (ret == nb) {//success
            ret = 0;
            if (isWriteFunction)
                printf("SUCCESS: written %d elements!\n", nb);
            else {
                printf("SUCCESS: read %d of elements:\n\tData: ", nb);
                int i = 0;
                if (DataInt == dataType) {
                    printf("0x%04x\n", data.dataInt);
                }
                else {
                    const char Format8[] = "0x%02x ";
                    const char Format16[] = "0x%04x ";
                    const char *format = ((Data8Array == dataType) ? Format8 : Format16);
                    for (; i < nb; ++i) {
                        printf(format, (Data8Array == dataType) ? data.data8[i] : data.data16[i]);
                    }
                    printf("\n");
                }
            }
        }
        else if (!addrScan){
            printf("ERROR occured, ret:%d, %s\n", ret, modbus_strerror(errno));
        }
    }
}
