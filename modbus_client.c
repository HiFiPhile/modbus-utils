/*
*  MIT License

*  Copyright (c) 2013  Krzysztow

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

#include <argtable3.h>

#include <modbus.h>
#include "errno.h"

#include "mbu-common.h"

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

int main(int argc, char **argv)
{
    int c;
    int ok;
    int readWriteNo = 1;
    int isWriteFunction = 0;
    modbus_t *ctx;

    enum WriteDataType {
        DataInt,
        Data8Array,
        Data16Array
    } wDataType = DataInt;
    union Data {
        int dataInt;
        uint8_t *data8;
        uint16_t *data16;
    } data;

    struct arg_int *addr   = arg_int1("a", "addr",              "<n>",                                  "Slave address");
    struct arg_int *reg    = arg_int1("r", "reg",               "<n>",                                  "Start register");
    struct arg_int *func   = arg_int1("f", "func",              "<n>",                                  "Modbus Function");
    struct arg_rem *func1  = arg_rem("",                                                                "0x01 : Read Coils");
    struct arg_rem *func2  = arg_rem("",                                                                "0x02 : Read Discrete Inputs");
    struct arg_rem *func3  = arg_rem("",                                                                "0x03 : Read Holding Registers");
    struct arg_rem *func4  = arg_rem("",                                                                "0x04 : Read Input Registers");
    struct arg_rem *func5  = arg_rem("",                                                                "0x05 : Write Single Coil");
    struct arg_rem *func6  = arg_rem("",                                                                "0x06 : Write Single Register");
    struct arg_rem *func7  = arg_rem("",                                                                "0x0F : Write Multiple Coils");
    struct arg_rem *func8  = arg_rem("",                                                                "0x10 : Write Multiple registers");
    struct arg_int *dwrite = arg_intn("w", "write",             "<n>", 0, 123,                          "Data to write");
    struct arg_int *count  = arg_int0("c", "count",             "<unit>",                               "Data read count");
    struct arg_int *tout   = arg_int0("o", "timeout",           "<ms>",                                 "Request timeout");
    struct arg_lit *debug  = arg_lit0("v", "verbose",                                                   "Enable verbpse output");
    struct arg_lit *help   = arg_lit0("h", "help",                                                      "Print this help and exit");
    /* RTU */
    struct arg_rex *rtu    = arg_rex1(NULL, NULL,   "RTU",      NULL,                   ARG_REX_ICASE,  NULL);
    struct arg_str *dev    = arg_str1("d", "dev",               "<device>",                             "Serial device");
    struct arg_int *baud   = arg_int1("b", "baud",              "<n>",                                  "Baud rate");
    struct arg_rex *dbit   = arg_rex0(NULL, "data-bits", "7|8",  "<7|8>=8",             ARG_REX_ICASE,  "Data bits");
    struct arg_rex *sbit   = arg_rex0(NULL, "stop-bits", "1|2",  "<1|2>=1",             ARG_REX_ICASE,  "Stop bits");
    struct arg_rex *parity = arg_rex0("p", "parity", "N|E|O",
                                                                "<N|E|O>=E", ARG_REX_ICASE,  "Parity");
    struct arg_end *end1    = arg_end(20);
    /* TCP */
    struct arg_rex *tcp    = arg_rex1(NULL, NULL,   "TCP",      NULL, ARG_REX_ICASE,                    NULL);
    struct arg_int *port   = arg_int0("p", "port",              "<port>=502",                           "Socket listening port");
    struct arg_rex *ip     = arg_rex0("i", "addr", "^([0-9]{1,3}\\.){3}([0-9]{1,3})$",
                                                                "<IP>=127.0.0.1",       ARG_REX_ICASE,  "Device IP address");
    struct arg_end *end2    = arg_end(20);

    void* argtable1[] = {rtu, addr, reg, func, func1, func2, func3, func4, func5, func6, func7, func8,
                            dev, baud, dbit, sbit, parity, dwrite, count, tout, debug, help, end1};

    void* argtable2[] = {tcp, addr, reg, func, func1, func2, func3, func4, func5, func6, func7, func8,
                            port, ip, dwrite, count, tout, debug, help, end2};

    /* defaults */
    count->ival[0]      = 1;
    tout->ival[0]       = 1000;
    dbit->sval[0]       = "8";
    sbit->sval[0]       = "1";
    parity->sval[0]     = "E";
    port->ival[0]       = 502;
    ip->sval[0]         = "127.0.0.1";

    int nerrors1 = arg_parse(argc,argv,argtable1);
    int nerrors2 = arg_parse(argc,argv,argtable2);
    /* special case: '--help' takes precedence over error reporting */
    if (help->count > 0)
    {
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
    if (rtu->count > 0)
    {
        if(nerrors1> 0) {
            /* Display the error details contained in the arg_end struct.*/
            arg_print_errors(stdout, end1, "modbus_client rtu");
            printf("Try '%s --help' for more information.\n", "modbus_client rtu");
            return -1;
        } else {
            ctx = modbus_new_rtu(dev->sval[0],
                baud->ival[0], toupper(parity->sval[0][0]), getInt(dbit->sval[0], 0), getInt(sbit->sval[0], 0));
        }
    } else if (tcp->count > 0)
    {
        if(nerrors2 > 0) {
            /* Display the error details contained in the arg_end struct.*/
            arg_print_errors(stdout, end2, "modbus_client tcp");
            printf("Try '%s --help' for more information.\n", "modbus_client tcp");
            return -1;
        } else {
            ctx = modbus_new_tcp(ip->sval[0], port->ival[0]);
        }
    } else {
        printf("Missing <rtu|tcp> command.\n");
        printf("usage 1: %s ", "modbus_client");  arg_print_syntax(stdout,argtable1,"\n");
        printf("usage 2: %s ", "modbus_client");  arg_print_syntax(stdout,argtable2,"\n");
        return -1;
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
        if (debug->count)
            printf("Data to write: ");
        for (int i = 0; i < dwrite->count; i++) {
            if (wDataType == DataInt) {
                data.dataInt = dwrite->ival[0];
                if (debug->count)
                    printf("0x%x", data.dataInt);
                /* only one data */
                break;
            } else if (wDataType == Data8Array) {
                data.data8[i] = dwrite->ival[i];
                if (debug->count)
                    printf("0x%02x ", data.data8[i]);
            } if (wDataType == Data16Array) {
                data.data16[i] = dwrite->ival[i];
                if (debug->count)
                    printf("0x%04x ", data.data16[i]);
            }
        }
        if (debug->count)
            printf("\n");
    }

    modbus_set_debug(ctx, debug->count);
    modbus_set_slave(ctx, addr->ival[0]);
    modbus_set_response_timeout(ctx, 0, tout->ival[0] * 1000);

    //issue the request
    int ret = -1;
    if (modbus_connect(ctx)) {
        fprintf(stderr, "Connection failed: %s\n",
                modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    switch (func->ival[0]) {
    case(ReadCoils):
        ret = modbus_read_bits(ctx, reg->ival[0], readWriteNo, data.data8);
        break;
    case(ReadDiscreteInput):
        printf("ReadDiscreteInput: not implemented yet!\n");
        wDataType = DataInt;
        break;
    case(ReadHoldingRegisters):
        ret = modbus_read_registers(ctx, reg->ival[0], readWriteNo, data.data16);
        break;
    case(ReadInputRegisters):
        ret = modbus_read_input_registers(ctx, reg->ival[0], readWriteNo, data.data16);
        break;
    case(WriteSingleCoil):
        ret = modbus_write_bit(ctx, reg->ival[0], data.dataInt);
        break;
    case(WriteSingleRegister):
        ret = modbus_write_register(ctx, reg->ival[0], data.dataInt);
        break;
    case(WriteMultipleCoils):
        ret = modbus_write_bits(ctx, reg->ival[0], readWriteNo, data.data8);
        break;
    case(WriteMultipleRegisters):
        ret = modbus_write_registers(ctx, reg->ival[0], readWriteNo, data.data16);
        break;
    default:
        break;
    }

    if (ret == readWriteNo) {//success
        ret = 0;
        if (isWriteFunction)
            printf("SUCCESS: written %d elements!\n", readWriteNo);
        else {
            printf("SUCCESS: read %d of elements:\n\tData: ", readWriteNo);
            int i = 0;
            if (DataInt == wDataType) {
                printf("0x%04x\n", data.dataInt);
            }
            else {
                const char Format8[] = "0x%02x ";
                const char Format16[] = "0x%04x ";
                const char *format = ((Data8Array == wDataType) ? Format8 : Format16);
                for (; i < readWriteNo; ++i) {
                    printf(format, (Data8Array == wDataType) ? data.data8[i] : data.data16[i]);
                }
                printf("\n");
            }
        }
    }
    else {
        printf("ERROR occured, ret:%d\n", ret);
        modbus_strerror(errno);
    }

    //cleanup
    modbus_close(ctx);
    modbus_free(ctx);

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

    exit(ret);
}
