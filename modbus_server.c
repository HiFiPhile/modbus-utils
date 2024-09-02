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

/*
 * The file is strongly based upon libmodbus/tests/random-test-server.c of libmodbus library
 */

#include <stdio.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <stdlib.h>
#include <getopt.h>

#include <modbus.h>
#include <errno.h>
#include <signal.h>

#include <argtable3.h>

#include "mbu-common.h"

#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define NB_CONNECTION    10

static modbus_t *ctx = NULL;
static modbus_mapping_t *mb_mapping;

static int server_socket = -1;

static void close_sigint(int dummy)
{
    if (server_socket != -1) {
        close(server_socket);
    }
    modbus_free(ctx);
    modbus_mapping_free(mb_mapping);

    exit(dummy);
}

int main(int argc, char **argv)
{
    int c;
    int ok;
    int rc;

    struct arg_int *addr   = arg_int0("a", "addr",              "<n>=1",                                "Slave address");
    struct arg_int *co     = arg_int0(NULL,"co",                "<n>=100",                              "Coils");
    struct arg_int *di     = arg_int0(NULL,"di",                "<n>=100",                              "Discrete inputs");
    struct arg_int *hr     = arg_int0(NULL,"hr",                "<n>=100",                              "Holding registers");
    struct arg_int *ir     = arg_int0(NULL,"ir",                "<n>=100",                              "Input registers");
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

    void* argtable1[] = {rtu, addr, co, di, hr, ir, dev, baud, dbit, sbit, parity, debug, help, end1};

    void* argtable2[] = {tcp, addr, co, di, hr, ir, port, ip, debug, help, end2};

    /* defaults */
    addr->ival[0] = 1;
    co->ival[0] = 100;
    di->ival[0] = 100;
    hr->ival[0] = 100;
    ir->ival[0] = 100;

    int nerrors1 = arg_parse(argc,argv,argtable1);
    int nerrors2 = arg_parse(argc,argv,argtable2);
    /* special case: '--help' takes precedence over error reporting */
    if (help->count) {
        printf("Modbus server utils.\n\n");
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
            arg_print_errors(stdout, end1, "modbus_server rtu");
            printf("Try '%s --help' for more information.\n", "modbus_server rtu");
            return -1;
        } else {
            ctx = modbus_new_rtu(dev->sval[0],
                baud->ival[0], toupper(parity->sval[0][0]), getInt(dbit->sval[0], 0), getInt(sbit->sval[0], 0));
        }
    } else if (tcp->count) {
        if(nerrors2) {
            /* Display the error details contained in the arg_end struct.*/
            arg_print_errors(stdout, end2, "modbus_server tcp");
            printf("Try '%s --help' for more information.\n", "modbus_server tcp");
            return -1;
        } else {
            ctx = modbus_new_tcp(ip->sval[0], port->ival[0]);
        }
    } else {
        printf("Missing <rtu|tcp> command.\n");
        printf("usage 1: %s ", "modbus_server");  arg_print_syntax(stdout,argtable1,"\n");
        printf("usage 2: %s ", "modbus_server");  arg_print_syntax(stdout,argtable2,"\n");
        return -1;
    }

    //prepare mapping
    mb_mapping = modbus_mapping_new(co->ival[0], di->ival[0], hr->ival[0], ir->ival[0]);
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n",
                modbus_strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (debug->count)
        printf("Ranges: \n \tCoils: 0-0x%04x\n\tDigital inputs: 0-0x%04x\n\tHolding registers: 0-0x%04x\n\tInput registers: 0-0x%04x\n",
               co->ival[0], di->ival[0], hr->ival[0], ir->ival[0]);

    modbus_set_debug(ctx, debug->count);
    modbus_set_slave(ctx, addr->ival[0]);

    if (rtu->count) {
        for(;;) {

            if (modbus_connect(ctx)) {
                break;
            }

            for (;;) {
                uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];

                rc = modbus_receive(ctx, query);
                if (rc > 0) {
                    /* rc is the query size */
                    modbus_reply(ctx, query, rc, mb_mapping);
                } else if (rc == -1) {
                    /* Connection closed by the client or error */
                    break;
                }
            }
            printf("Client disconnected: %s\n", modbus_strerror(errno));
        }
    } else {
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        int master_socket;
        fd_set refset;
        fd_set rdset;
        /* Maximum file descriptor number */
        int fdmax;

        server_socket = modbus_tcp_listen(ctx, NB_CONNECTION);
        if (server_socket == -1) {
            fprintf(stderr, "Unable to listen TCP connection\n");
            modbus_free(ctx);
            return -1;
        }

        signal(SIGINT, close_sigint);

        /* Clear the reference set of socket */
        FD_ZERO(&refset);
        /* Add the server socket */
        FD_SET(server_socket, &refset);

        /* Keep track of the max file descriptor */
        fdmax = server_socket;

        for (;;) {
            rdset = refset;
            if (select(fdmax+1, &rdset, NULL, NULL, NULL) == -1) {
                perror("Server select() failure.");
                close_sigint(1);
            }

            /* Run through the existing connections looking for data to be
             * read */
            for (master_socket = 0; master_socket <= fdmax; master_socket++) {

                if (!FD_ISSET(master_socket, &rdset)) {
                    continue;
                }

                if (master_socket == server_socket) {
                    /* A client is asking a new connection */
                    socklen_t addrlen;
                    struct sockaddr_in clientaddr;
                    int newfd;

                    /* Handle new connections */
                    addrlen = sizeof(clientaddr);
                    memset(&clientaddr, 0, sizeof(clientaddr));
                    newfd = accept(server_socket, (struct sockaddr *)&clientaddr, &addrlen);
                    if (newfd == -1) {
                        perror("Server accept() error");
                    } else {
                        FD_SET(newfd, &refset);

                        if (newfd > fdmax) {
                            /* Keep track of the maximum */
                            fdmax = newfd;
                        }
                        printf("New connection from %s:%d on socket %d\n",
                            inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
                    }
                } else {
                    modbus_set_socket(ctx, master_socket);
                    rc = modbus_receive(ctx, query);
                    if (rc > 0) {
                        modbus_reply(ctx, query, rc, mb_mapping);
                    } else if (rc == -1) {
                        /* This example server in ended on connection closing or
                         * any errors. */
                        printf("Connection closed on socket %d\n", master_socket);
                        close(master_socket);

                        /* Remove from reference set */
                        FD_CLR(master_socket, &refset);

                        if (master_socket == fdmax) {
                            fdmax--;
                        }
                    }
                }
            }
        }
    }

    modbus_mapping_free(mb_mapping);
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}
