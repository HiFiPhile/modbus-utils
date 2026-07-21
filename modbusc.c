/*
 * MIT License
 *
 * Copyright (c) 2013 Krzysztow (original author)
 * Copyright (c) 2024-2026 Zixun LI (rewrite and maintenance)
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <argtable3.h>
#include <modbus.h>

#include "mbu-common.h"

#define PROGRAM_NAME "modbusc"

typedef enum { DATA_SCALAR, DATA_BITS, DATA_REGISTERS } data_type_t;

typedef union {
    int scalar;
    uint8_t *bits;
    uint16_t *registers;
} request_data_t;

static int equals_ignore_case(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == *right;
}

static int validate_register_range(int address, int count) {
    return address >= 0 && address <= MBU_MAX_DATA_ADDRESS && count > 0 &&
           count - 1 <= MBU_MAX_DATA_ADDRESS - address;
}

static int configure_context(modbus_t *ctx, int debug, uint32_t timeout_seconds,
                             uint32_t timeout_microseconds) {
    if (ctx == NULL) {
        fprintf(stderr, "%s: unable to create Modbus context: %s\n", PROGRAM_NAME,
                modbus_strerror(errno));
        return -1;
    }
    if (modbus_set_debug(ctx, debug) == -1 ||
        modbus_set_response_timeout(ctx, timeout_seconds, timeout_microseconds) == -1) {
        fprintf(stderr, "%s: unable to configure Modbus context: %s\n", PROGRAM_NAME,
                modbus_strerror(errno));
        return -1;
    }
    return 0;
}

static int process_requests(modbus_t *ctx, int address_start, int address_end, int function,
                            int register_address, int count, data_type_t data_type,
                            const request_data_t *data, const char *prefix,
                            unsigned int interframe_delay, int verbose) {
    int successes = 0;
    bool address_scan = address_start != address_end;
    bool write_function = function == MODBUS_FC_WRITE_SINGLE_COIL ||
                          function == MODBUS_FC_WRITE_SINGLE_REGISTER ||
                          function == MODBUS_FC_WRITE_MULTIPLE_COILS ||
                          function == MODBUS_FC_WRITE_MULTIPLE_REGISTERS;

    for (int address = address_start; address <= address_end; address++) {
        int result = -1;

        if (modbus_set_slave(ctx, address) == -1) {
            if (!address_scan || verbose) {
                fprintf(stderr, "%sAddress:%d: unable to select slave: %s\n", prefix, address,
                        modbus_strerror(errno));
            }
            continue;
        }

        switch (function) {
        case MODBUS_FC_READ_COILS:
            result = modbus_read_bits(ctx, register_address, count, data->bits);
            break;
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            result = modbus_read_input_bits(ctx, register_address, count, data->bits);
            break;
        case MODBUS_FC_READ_HOLDING_REGISTERS:
            result = modbus_read_registers(ctx, register_address, count, data->registers);
            break;
        case MODBUS_FC_READ_INPUT_REGISTERS:
            result = modbus_read_input_registers(ctx, register_address, count, data->registers);
            break;
        case MODBUS_FC_WRITE_SINGLE_COIL:
            result = modbus_write_bit(ctx, register_address, data->scalar);
            break;
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            result = modbus_write_register(ctx, register_address, (uint16_t)data->scalar);
            break;
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
            result = modbus_write_bits(ctx, register_address, count, data->bits);
            break;
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            result = modbus_write_registers(ctx, register_address, count, data->registers);
            break;
        default:
            break;
        }

        if (result == count) {
            successes++;
            if (address_scan) {
                printf("%sAddress:%d\n", prefix, address);
            }
            if (write_function) {
                printf("SUCCESS: written %d element%s\n", count, count == 1 ? "" : "s");
            } else {
                printf("SUCCESS: read %d element%s:\n\tData: ", count, count == 1 ? "" : "s");
                for (int index = 0; index < count; index++) {
                    if (data_type == DATA_BITS) {
                        printf("0x%02x ", (unsigned int)data->bits[index]);
                    } else {
                        printf("0x%04x ", (unsigned int)data->registers[index]);
                    }
                }
                printf("\n");
            }
        } else if (!address_scan || verbose) {
            fprintf(stderr, "%sAddress:%d: request failed (result %d): %s\n", prefix, address,
                    result, modbus_strerror(errno));
        }

        if (address != address_end) {
            mbu_sleep_ms(interframe_delay);
        }
    }

    return successes > 0 ? 0 : -1;
}

static int connect_and_process(modbus_t *ctx, int address_start, int address_end, int function,
                               int register_address, int count, data_type_t data_type,
                               const request_data_t *data, const char *prefix,
                               unsigned int interframe_delay, int verbose, uint32_t timeout_seconds,
                               uint32_t timeout_microseconds) {
    int result;

    if (configure_context(ctx, verbose > 1, timeout_seconds, timeout_microseconds) == -1) {
        modbus_free(ctx);
        return -1;
    }
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "%s%sconnection failed: %s\n", prefix, *prefix == '\0' ? "" : ": ",
                modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    result = process_requests(ctx, address_start, address_end, function, register_address, count,
                              data_type, data, prefix, interframe_delay, verbose);
    modbus_close(ctx);
    modbus_free(ctx);
    return result;
}

static void print_general_help(void) {
    printf("Modbus client utility.\n\n");
    printf("Usage:\n");
    printf("  %s rtu [options]\n", PROGRAM_NAME);
    printf("  %s tcp [options]\n\n", PROGRAM_NAME);
    printf("Run '%s <rtu|tcp> --help' for transport-specific options.\n", PROGRAM_NAME);
}

int main(int argc, char **argv) {
    int exit_code = EXIT_FAILURE;
    int address_start;
    int address_end;
    int register_address;
    int request_count;
    int function_value;
    int verbose;
    bool is_rtu;
    bool is_write = false;
    data_type_t data_type = DATA_SCALAR;
    request_data_t data = {0};
    uint32_t timeout_seconds;
    uint32_t timeout_microseconds;

    struct arg_rex *address = arg_rex1("a", "addr", "^[0-9]{1,3}(\\.[0-9]{1,3})?$", "<n>|<n.n>", 0,
                                       "Slave address or inclusive scan range");
    struct arg_int *reg = arg_int1("r", "reg", "<n>", "Start register");
    struct arg_int *function = arg_int1("f", "func", "<n>", "Modbus function code");
    struct arg_int *write_data =
        arg_intn("w", "write", "<n>", 0, MODBUS_MAX_WRITE_BITS, "Data to write");
    struct arg_int *count = arg_int0("c", "count", "<n>=1", "Number of values to read");
    struct arg_int *timeout = arg_int0("o", "timeout", "<ms>=1000", "Positive response timeout");
    struct arg_lit *base_one = arg_lit0("1", "base-1", "Use base-1 register addressing");
    struct arg_int *interframe_delay =
        arg_int0("e", "itf-delay", "<ms>=200", "Non-negative interframe delay");
    struct arg_lit *debug = arg_litn("v", "verbose", 0, 2, "Increase output verbosity");
    struct arg_lit *help = arg_lit0("h", "help", "Print help and exit");
    struct arg_lit *version = arg_lit0(NULL, "version", "Print version and exit");
    struct arg_rex *rtu = arg_rex1(NULL, NULL, "rtu", NULL, ARG_REX_ICASE, NULL);
    struct arg_str *device = arg_str1("d", "dev", "<device>", "Serial device");
    struct arg_int *baud = arg_intn("b", "baud", "<n>", 1, 16, "Positive baud rate");
    struct arg_rex *data_bits = arg_rex0(NULL, "data-bits", "^(7|8)$", "<7|8>=8", 0, "Data bits");
    struct arg_rex *stop_bits = arg_rex0(NULL, "stop-bits", "^(1|2)$", "<1|2>=1", 0, "Stop bits");
    struct arg_rex *parity = arg_rexn("p", "parity", "^(N|E|O)$", "<N|E|O>=E", 0, 3, ARG_REX_ICASE,
                                      "Parity; repeat to scan");
    struct arg_end *rtu_end = arg_end(20);
    struct arg_rex *tcp = arg_rex1(NULL, NULL, "tcp", NULL, ARG_REX_ICASE, NULL);
    struct arg_int *port = arg_int0("p", "port", "<port>=502", "TCP port");
    struct arg_str *host = arg_str0("i", "ip", "<IPv4>=127.0.0.1", "Device IPv4 address");
    struct arg_end *tcp_end = arg_end(20);

    void *rtu_table[] = {rtu,       address,          reg,    function, write_data, count,  timeout,
                         base_one,  interframe_delay, debug,  help,     version,    device, baud,
                         data_bits, stop_bits,        parity, rtu_end};
    void *tcp_table[] = {tcp,      address,          reg,   function, write_data, count, timeout,
                         base_one, interframe_delay, debug, help,     version,    port,  host,
                         tcp_end};
    void *all_args[] = {address,          reg,       function, write_data, count, timeout, base_one,
                        interframe_delay, debug,     help,     version,    rtu,   device,  baud,
                        data_bits,        stop_bits, parity,   rtu_end,    tcp,   port,    host,
                        tcp_end};
    void **selected_table;
    struct arg_end *selected_end;

    if (arg_nullcheck(rtu_table) != 0 || arg_nullcheck(tcp_table) != 0) {
        fprintf(stderr, "%s: insufficient memory for argument parsing\n", PROGRAM_NAME);
        arg_freetable(all_args, sizeof(all_args) / sizeof(all_args[0]));
        return EXIT_FAILURE;
    }

    count->ival[0] = 1;
    timeout->ival[0] = 1000;
    interframe_delay->ival[0] = 200;
    data_bits->sval[0] = "8";
    stop_bits->sval[0] = "1";
    port->ival[0] = 502;
    host->sval[0] = "127.0.0.1";

    if (argc == 2 && (equals_ignore_case(argv[1], "--help") || equals_ignore_case(argv[1], "-h"))) {
        print_general_help();
        exit_code = EXIT_SUCCESS;
        goto cleanup_args;
    }
    if (argc == 2 && equals_ignore_case(argv[1], "--version")) {
        printf("%s %s\n", PROGRAM_NAME, MBU_VERSION);
        exit_code = EXIT_SUCCESS;
        goto cleanup_args;
    }
    if (argc < 2 || (!equals_ignore_case(argv[1], "rtu") && !equals_ignore_case(argv[1], "tcp"))) {
        fprintf(stderr, "%s: missing or invalid <rtu|tcp> command\n", PROGRAM_NAME);
        print_general_help();
        goto cleanup_args;
    }

    is_rtu = equals_ignore_case(argv[1], "rtu");
    if (is_rtu) {
        selected_table = rtu_table;
        selected_end = rtu_end;
    } else {
        selected_table = tcp_table;
        selected_end = tcp_end;
    }

    int parse_errors = arg_parse(argc, argv, selected_table);
    if (version->count != 0) {
        printf("%s %s\n", PROGRAM_NAME, MBU_VERSION);
        exit_code = EXIT_SUCCESS;
        goto cleanup_args;
    }
    if (help->count != 0) {
        printf("Modbus client utility.\n\n");
        arg_print_syntax(stdout, selected_table, "\n");
        arg_print_glossary(stdout, selected_table, "  %-30s %s\n");
        exit_code = EXIT_SUCCESS;
        goto cleanup_args;
    }
    if (parse_errors != 0) {
        arg_print_errors(stderr, selected_end, PROGRAM_NAME);
        fprintf(stderr, "Try '%s %s --help' for more information.\n", PROGRAM_NAME,
                is_rtu ? "rtu" : "tcp");
        goto cleanup_args;
    }

    if (parity->count == 0) {
        parity->count = 1;
        parity->sval[0] = "E";
    }
    if (mbu_parse_address_range(address->sval[0], &address_start, &address_end) == -1) {
        fprintf(stderr, "%s: slave address must be 0..%d or an increasing range n.n\n",
                PROGRAM_NAME, MBU_MAX_SLAVE_ADDRESS);
        goto cleanup_args;
    }

    register_address = reg->ival[0];
    if (base_one->count != 0) {
        register_address--;
    }
    function_value = function->ival[0];
    switch (function_value) {
    case MODBUS_FC_READ_COILS:
    case MODBUS_FC_READ_DISCRETE_INPUTS:
        data_type = DATA_BITS;
        request_count = count->ival[0];
        if (request_count < 1 || request_count > MODBUS_MAX_READ_BITS) {
            fprintf(stderr, "%s: bit read count must be 1..%d\n", PROGRAM_NAME,
                    MODBUS_MAX_READ_BITS);
            goto cleanup_args;
        }
        break;
    case MODBUS_FC_READ_HOLDING_REGISTERS:
    case MODBUS_FC_READ_INPUT_REGISTERS:
        data_type = DATA_REGISTERS;
        request_count = count->ival[0];
        if (request_count < 1 || request_count > MODBUS_MAX_READ_REGISTERS) {
            fprintf(stderr, "%s: register read count must be 1..%d\n", PROGRAM_NAME,
                    MODBUS_MAX_READ_REGISTERS);
            goto cleanup_args;
        }
        break;
    case MODBUS_FC_WRITE_SINGLE_COIL:
    case MODBUS_FC_WRITE_SINGLE_REGISTER:
        data_type = DATA_SCALAR;
        is_write = true;
        request_count = 1;
        if (write_data->count != 1) {
            fprintf(stderr, "%s: single-value writes require exactly one --write value\n",
                    PROGRAM_NAME);
            goto cleanup_args;
        }
        break;
    case MODBUS_FC_WRITE_MULTIPLE_COILS:
        data_type = DATA_BITS;
        is_write = true;
        request_count = write_data->count;
        if (request_count < 1 || request_count > MODBUS_MAX_WRITE_BITS) {
            fprintf(stderr, "%s: multiple-coil writes require 1..%d values\n", PROGRAM_NAME,
                    MODBUS_MAX_WRITE_BITS);
            goto cleanup_args;
        }
        break;
    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        data_type = DATA_REGISTERS;
        is_write = true;
        request_count = write_data->count;
        if (request_count < 1 || request_count > MODBUS_MAX_WRITE_REGISTERS) {
            fprintf(stderr, "%s: multiple-register writes require 1..%d values\n", PROGRAM_NAME,
                    MODBUS_MAX_WRITE_REGISTERS);
            goto cleanup_args;
        }
        break;
    default:
        fprintf(stderr, "%s: unsupported function 0x%02x\n", PROGRAM_NAME, function_value);
        goto cleanup_args;
    }

    if (!is_write && write_data->count != 0) {
        fprintf(stderr, "%s: --write is only valid for write functions\n", PROGRAM_NAME);
        goto cleanup_args;
    }
    if (!validate_register_range(register_address, request_count)) {
        fprintf(stderr, "%s: register range must fit within 0..%d\n", PROGRAM_NAME,
                MBU_MAX_DATA_ADDRESS);
        goto cleanup_args;
    }
    if (timeout->ival[0] <= 0 ||
        mbu_timeout_from_ms(timeout->ival[0], &timeout_seconds, &timeout_microseconds) == -1) {
        fprintf(stderr, "%s: timeout must be a positive number of milliseconds\n", PROGRAM_NAME);
        goto cleanup_args;
    }
    if (interframe_delay->ival[0] < 0) {
        fprintf(stderr, "%s: interframe delay must be non-negative\n", PROGRAM_NAME);
        goto cleanup_args;
    }
    if (!is_rtu && (port->ival[0] < 1 || port->ival[0] > 65535)) {
        fprintf(stderr, "%s: TCP port must be 1..65535\n", PROGRAM_NAME);
        goto cleanup_args;
    }
    if (is_rtu) {
        for (int index = 0; index < baud->count; index++) {
            if (baud->ival[index] <= 0) {
                fprintf(stderr, "%s: baud rate must be positive\n", PROGRAM_NAME);
                goto cleanup_args;
            }
        }
    }

    if (data_type == DATA_BITS) {
        data.bits = calloc((size_t)request_count, sizeof(*data.bits));
        if (data.bits == NULL) {
            fprintf(stderr, "%s: unable to allocate request buffer\n", PROGRAM_NAME);
            goto cleanup_args;
        }
    } else if (data_type == DATA_REGISTERS) {
        data.registers = calloc((size_t)request_count, sizeof(*data.registers));
        if (data.registers == NULL) {
            fprintf(stderr, "%s: unable to allocate request buffer\n", PROGRAM_NAME);
            goto cleanup_data;
        }
    }

    if (is_write) {
        for (int index = 0; index < request_count; index++) {
            int value = write_data->ival[index];
            if (data_type == DATA_BITS) {
                if (value != 0 && value != 1) {
                    fprintf(stderr, "%s: coil values must be 0 or 1\n", PROGRAM_NAME);
                    goto cleanup_data;
                }
                data.bits[index] = (uint8_t)value;
            } else if (data_type == DATA_REGISTERS) {
                if (value < 0 || value > UINT16_MAX) {
                    fprintf(stderr, "%s: register values must be 0..%u\n", PROGRAM_NAME,
                            (unsigned int)UINT16_MAX);
                    goto cleanup_data;
                }
                data.registers[index] = (uint16_t)value;
            } else {
                if (function_value == MODBUS_FC_WRITE_SINGLE_COIL && value != 0 && value != 1) {
                    fprintf(stderr, "%s: coil values must be 0 or 1\n", PROGRAM_NAME);
                    goto cleanup_data;
                }
                if (function_value == MODBUS_FC_WRITE_SINGLE_REGISTER &&
                    (value < 0 || value > UINT16_MAX)) {
                    fprintf(stderr, "%s: register values must be 0..%u\n", PROGRAM_NAME,
                            (unsigned int)UINT16_MAX);
                    goto cleanup_data;
                }
                data.scalar = value;
            }
        }
    }

    verbose = debug->count;
    if (is_rtu) {
        int successes = 0;
        bool scan_settings = baud->count > 1 || parity->count > 1;

        for (int baud_index = 0; baud_index < baud->count; baud_index++) {
            for (int parity_index = 0; parity_index < parity->count; parity_index++) {
                char prefix[64] = "";
                char parity_value = (char)toupper((unsigned char)parity->sval[parity_index][0]);
                modbus_t *ctx;

                if (scan_settings) {
                    snprintf(prefix, sizeof(prefix), "Baud:%d Parity:%c ", baud->ival[baud_index],
                             parity_value);
                }
                ctx = modbus_new_rtu(device->sval[0], baud->ival[baud_index], parity_value,
                                     data_bits->sval[0][0] - '0', stop_bits->sval[0][0] - '0');
                if (connect_and_process(ctx, address_start, address_end, function_value,
                                        register_address, request_count, data_type, &data, prefix,
                                        (unsigned int)interframe_delay->ival[0], verbose,
                                        timeout_seconds, timeout_microseconds) == 0) {
                    successes++;
                }
            }
        }
        if (successes > 0) {
            exit_code = EXIT_SUCCESS;
        }
    } else {
        modbus_t *ctx = modbus_new_tcp(host->sval[0], port->ival[0]);
        if (connect_and_process(ctx, address_start, address_end, function_value, register_address,
                                request_count, data_type, &data, "",
                                (unsigned int)interframe_delay->ival[0], verbose, timeout_seconds,
                                timeout_microseconds) == 0) {
            exit_code = EXIT_SUCCESS;
        }
    }

cleanup_data:
    if (data_type == DATA_BITS) {
        free(data.bits);
    } else if (data_type == DATA_REGISTERS) {
        free(data.registers);
    }
cleanup_args:
    arg_freetable(all_args, sizeof(all_args) / sizeof(all_args[0]));
    return exit_code;
}
