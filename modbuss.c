/*
 * MIT License
 *
 * Copyright (c) 2013 Krzysztow (original author)
 * Copyright (c) 2024-2026 Zixun LI (rewrite and maintenance)
 *
 * The server loop is based on libmodbus's random-test-server example.
 */

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <argtable3.h>
#include <modbus.h>

#include "mbu-common.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define PROGRAM_NAME "modbuss"
#define MAX_CONNECTIONS 10
#define MAX_MAPPING_SIZE 65536

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

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

static void close_socket_fd(int socket_fd) {
#if defined(_WIN32)
    closesocket((SOCKET)socket_fd);
#else
    close(socket_fd);
#endif
}

static int run_rtu_server(modbus_t *ctx, modbus_mapping_t *mapping) {
    uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];

    while (!stop_requested) {
        if (modbus_connect(ctx) == -1) {
            if (stop_requested && errno == EINTR) {
                break;
            }
            fprintf(stderr, "%s: RTU connection failed: %s\n", PROGRAM_NAME,
                    modbus_strerror(errno));
            return -1;
        }

        while (!stop_requested) {
            int request_length = modbus_receive(ctx, query);

            if (request_length > 0) {
                if (modbus_reply(ctx, query, request_length, mapping) == -1) {
                    fprintf(stderr, "%s: RTU reply failed: %s\n", PROGRAM_NAME,
                            modbus_strerror(errno));
                    break;
                }
            } else if (request_length == -1) {
                if (!stop_requested && errno != EINTR) {
                    fprintf(stderr, "%s: RTU client disconnected: %s\n", PROGRAM_NAME,
                            modbus_strerror(errno));
                }
                break;
            }
        }
        modbus_close(ctx);
    }

    return 0;
}

static int run_tcp_server(modbus_t *ctx, modbus_mapping_t *mapping) {
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    fd_set connections;
    int listener;
    int highest_socket;
    int status = 0;

    listener = modbus_tcp_listen(ctx, MAX_CONNECTIONS);
    if (listener == -1) {
        fprintf(stderr, "%s: unable to listen for TCP connections: %s\n", PROGRAM_NAME,
                modbus_strerror(errno));
        return -1;
    }

    FD_ZERO(&connections);
    FD_SET(listener, &connections);
    highest_socket = listener;

    while (!stop_requested) {
        fd_set ready = connections;
        int selected = select(highest_socket + 1, &ready, NULL, NULL, NULL);

        if (selected == -1) {
            if (errno == EINTR && stop_requested) {
                break;
            }
            fprintf(stderr, "%s: select failed: %s\n", PROGRAM_NAME, strerror(errno));
            status = -1;
            break;
        }

        for (int socket_fd = 0; socket_fd <= highest_socket && selected > 0; socket_fd++) {
            if (!FD_ISSET(socket_fd, &ready)) {
                continue;
            }
            selected--;

            if (socket_fd == listener) {
                struct sockaddr_in client_address;
                socklen_t address_length = sizeof(client_address);
                int client = accept(listener, (struct sockaddr *)&client_address, &address_length);

                if (client == -1) {
                    if (errno != EINTR) {
                        fprintf(stderr, "%s: accept failed: %s\n", PROGRAM_NAME, strerror(errno));
                    }
                    continue;
                }
                if (client >= FD_SETSIZE) {
                    fprintf(stderr, "%s: rejecting socket outside select() capacity\n",
                            PROGRAM_NAME);
                    close_socket_fd(client);
                    continue;
                }

                FD_SET(client, &connections);
                if (client > highest_socket) {
                    highest_socket = client;
                }
                printf("New connection from %s:%u on socket %d\n",
                       inet_ntoa(client_address.sin_addr),
                       (unsigned int)ntohs(client_address.sin_port), client);
            } else {
                int request_length;

                if (modbus_set_socket(ctx, socket_fd) == -1) {
                    fprintf(stderr, "%s: unable to select socket %d: %s\n", PROGRAM_NAME, socket_fd,
                            modbus_strerror(errno));
                    request_length = -1;
                } else {
                    request_length = modbus_receive(ctx, query);
                }

                if (request_length > 0) {
                    if (modbus_reply(ctx, query, request_length, mapping) == -1) {
                        fprintf(stderr, "%s: reply failed on socket %d: %s\n", PROGRAM_NAME,
                                socket_fd, modbus_strerror(errno));
                        request_length = -1;
                    }
                }
                if (request_length == -1) {
                    printf("Connection closed on socket %d\n", socket_fd);
                    close_socket_fd(socket_fd);
                    FD_CLR(socket_fd, &connections);
                    while (highest_socket > listener && !FD_ISSET(highest_socket, &connections)) {
                        highest_socket--;
                    }
                }
            }
        }
    }

    for (int socket_fd = 0; socket_fd <= highest_socket; socket_fd++) {
        if (FD_ISSET(socket_fd, &connections)) {
            close_socket_fd(socket_fd);
        }
    }
    return status;
}

static void print_general_help(void) {
    printf("Modbus server utility.\n\n");
    printf("Usage:\n");
    printf("  %s rtu [options]\n", PROGRAM_NAME);
    printf("  %s tcp [options]\n\n", PROGRAM_NAME);
    printf("Run '%s <rtu|tcp> --help' for transport-specific options.\n", PROGRAM_NAME);
}

int main(int argc, char **argv) {
    int exit_code = EXIT_FAILURE;
    bool is_rtu;
    modbus_t *ctx = NULL;
    modbus_mapping_t *mapping = NULL;

    struct arg_int *address = arg_int0("a", "addr", "<n>=1", "Slave address (1..247)");
    struct arg_int *coils = arg_int0(NULL, "co", "<n>=100", "Number of coils");
    struct arg_int *discrete_inputs = arg_int0(NULL, "di", "<n>=100", "Number of discrete inputs");
    struct arg_int *holding_registers =
        arg_int0(NULL, "hr", "<n>=100", "Number of holding registers");
    struct arg_int *input_registers = arg_int0(NULL, "ir", "<n>=100", "Number of input registers");
    struct arg_lit *debug = arg_lit0("v", "verbose", "Enable verbose protocol output");
    struct arg_lit *help = arg_lit0("h", "help", "Print help and exit");
    struct arg_lit *version = arg_lit0(NULL, "version", "Print version and exit");
    struct arg_rex *rtu = arg_rex1(NULL, NULL, "rtu", NULL, ARG_REX_ICASE, NULL);
    struct arg_str *device = arg_str1("d", "dev", "<device>", "Serial device");
    struct arg_int *baud = arg_int1("b", "baud", "<n>", "Positive baud rate");
    struct arg_rex *data_bits = arg_rex0(NULL, "data-bits", "^(7|8)$", "<7|8>=8", 0, "Data bits");
    struct arg_rex *stop_bits = arg_rex0(NULL, "stop-bits", "^(1|2)$", "<1|2>=1", 0, "Stop bits");
    struct arg_rex *parity =
        arg_rex0("p", "parity", "^(N|E|O)$", "<N|E|O>=E", ARG_REX_ICASE, "Parity");
    struct arg_end *rtu_end = arg_end(20);
    struct arg_rex *tcp = arg_rex1(NULL, NULL, "tcp", NULL, ARG_REX_ICASE, NULL);
    struct arg_int *port = arg_int0("p", "port", "<port>=502", "TCP listening port");
    struct arg_str *bind_address =
        arg_str0("i", "ip", "<IP>=127.0.0.1", "IPv4 address to bind; use 0.0.0.0 for all");
    struct arg_end *tcp_end = arg_end(20);

    void *rtu_table[] = {rtu,
                         address,
                         coils,
                         discrete_inputs,
                         holding_registers,
                         input_registers,
                         device,
                         baud,
                         data_bits,
                         stop_bits,
                         parity,
                         debug,
                         help,
                         version,
                         rtu_end};
    void *tcp_table[] = {tcp,
                         address,
                         coils,
                         discrete_inputs,
                         holding_registers,
                         input_registers,
                         port,
                         bind_address,
                         debug,
                         help,
                         version,
                         tcp_end};
    void *all_args[] = {address,
                        coils,
                        discrete_inputs,
                        holding_registers,
                        input_registers,
                        debug,
                        help,
                        version,
                        rtu,
                        device,
                        baud,
                        data_bits,
                        stop_bits,
                        parity,
                        rtu_end,
                        tcp,
                        port,
                        bind_address,
                        tcp_end};
    void **selected_table;
    struct arg_end *selected_end;

    if (arg_nullcheck(rtu_table) != 0 || arg_nullcheck(tcp_table) != 0) {
        fprintf(stderr, "%s: insufficient memory for argument parsing\n", PROGRAM_NAME);
        arg_freetable(all_args, sizeof(all_args) / sizeof(all_args[0]));
        return EXIT_FAILURE;
    }

    address->ival[0] = 1;
    coils->ival[0] = 100;
    discrete_inputs->ival[0] = 100;
    holding_registers->ival[0] = 100;
    input_registers->ival[0] = 100;
    data_bits->sval[0] = "8";
    stop_bits->sval[0] = "1";
    parity->sval[0] = "E";
    port->ival[0] = 502;
    bind_address->sval[0] = "127.0.0.1";

    if (argc == 2 && (equals_ignore_case(argv[1], "--help") || equals_ignore_case(argv[1], "-h"))) {
        print_general_help();
        exit_code = EXIT_SUCCESS;
        goto cleanup;
    }
    if (argc == 2 && equals_ignore_case(argv[1], "--version")) {
        printf("%s %s\n", PROGRAM_NAME, MBU_VERSION);
        exit_code = EXIT_SUCCESS;
        goto cleanup;
    }
    if (argc < 2 || (!equals_ignore_case(argv[1], "rtu") && !equals_ignore_case(argv[1], "tcp"))) {
        fprintf(stderr, "%s: missing or invalid <rtu|tcp> command\n", PROGRAM_NAME);
        print_general_help();
        goto cleanup;
    }

    is_rtu = equals_ignore_case(argv[1], "rtu");
    selected_table = is_rtu ? rtu_table : tcp_table;
    selected_end = is_rtu ? rtu_end : tcp_end;

    int parse_errors = arg_parse(argc, argv, selected_table);
    if (version->count != 0) {
        printf("%s %s\n", PROGRAM_NAME, MBU_VERSION);
        exit_code = EXIT_SUCCESS;
        goto cleanup;
    }
    if (help->count != 0) {
        printf("Modbus server utility.\n\n");
        arg_print_syntax(stdout, selected_table, "\n");
        arg_print_glossary(stdout, selected_table, "  %-30s %s\n");
        exit_code = EXIT_SUCCESS;
        goto cleanup;
    }
    if (parse_errors != 0) {
        arg_print_errors(stderr, selected_end, PROGRAM_NAME);
        fprintf(stderr, "Try '%s %s --help' for more information.\n", PROGRAM_NAME,
                is_rtu ? "rtu" : "tcp");
        goto cleanup;
    }

    if (address->ival[0] < 1 || address->ival[0] > MBU_MAX_SLAVE_ADDRESS) {
        fprintf(stderr, "%s: slave address must be 1..%d\n", PROGRAM_NAME, MBU_MAX_SLAVE_ADDRESS);
        goto cleanup;
    }
    if (coils->ival[0] < 1 || coils->ival[0] > MAX_MAPPING_SIZE || discrete_inputs->ival[0] < 1 ||
        discrete_inputs->ival[0] > MAX_MAPPING_SIZE || holding_registers->ival[0] < 1 ||
        holding_registers->ival[0] > MAX_MAPPING_SIZE || input_registers->ival[0] < 1 ||
        input_registers->ival[0] > MAX_MAPPING_SIZE) {
        fprintf(stderr, "%s: mapping sizes must be 1..%d\n", PROGRAM_NAME, MAX_MAPPING_SIZE);
        goto cleanup;
    }
    if (!is_rtu && (port->ival[0] < 1 || port->ival[0] > 65535)) {
        fprintf(stderr, "%s: TCP port must be 1..65535\n", PROGRAM_NAME);
        goto cleanup;
    }
    if (is_rtu && baud->ival[0] <= 0) {
        fprintf(stderr, "%s: baud rate must be positive\n", PROGRAM_NAME);
        goto cleanup;
    }

    if (is_rtu) {
        char parity_value = (char)toupper((unsigned char)parity->sval[0][0]);
        ctx = modbus_new_rtu(device->sval[0], baud->ival[0], parity_value,
                             data_bits->sval[0][0] - '0', stop_bits->sval[0][0] - '0');
    } else {
        ctx = modbus_new_tcp(bind_address->sval[0], port->ival[0]);
    }
    if (ctx == NULL) {
        fprintf(stderr, "%s: unable to create Modbus context: %s\n", PROGRAM_NAME,
                modbus_strerror(errno));
        goto cleanup;
    }
    if (modbus_set_debug(ctx, debug->count) == -1 ||
        modbus_set_slave(ctx, address->ival[0]) == -1) {
        fprintf(stderr, "%s: unable to configure Modbus context: %s\n", PROGRAM_NAME,
                modbus_strerror(errno));
        goto cleanup;
    }

    mapping = modbus_mapping_new(coils->ival[0], discrete_inputs->ival[0],
                                 holding_registers->ival[0], input_registers->ival[0]);
    if (mapping == NULL) {
        fprintf(stderr, "%s: unable to allocate mapping: %s\n", PROGRAM_NAME,
                modbus_strerror(errno));
        goto cleanup;
    }

    if (debug->count != 0) {
        printf("Ranges:\n\tCoils: 0-0x%04x\n\tDiscrete inputs: 0-0x%04x\n"
               "\tHolding registers: 0-0x%04x\n\tInput registers: 0-0x%04x\n",
               coils->ival[0] - 1, discrete_inputs->ival[0] - 1, holding_registers->ival[0] - 1,
               input_registers->ival[0] - 1);
    }

    if (signal(SIGINT, request_stop) == SIG_ERR || signal(SIGTERM, request_stop) == SIG_ERR) {
        fprintf(stderr, "%s: unable to install signal handlers: %s\n", PROGRAM_NAME,
                strerror(errno));
        goto cleanup;
    }

    if ((is_rtu ? run_rtu_server(ctx, mapping) : run_tcp_server(ctx, mapping)) == 0) {
        exit_code = EXIT_SUCCESS;
    }

cleanup:
    modbus_mapping_free(mapping);
    modbus_free(ctx);
    arg_freetable(all_args, sizeof(all_args) / sizeof(all_args[0]));
    return exit_code;
}
