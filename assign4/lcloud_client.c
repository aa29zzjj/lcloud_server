////////////////////////////////////////////////////////////////////////////////
//
//  File          : lcloud_client.c
//  Description   : This is the client side of the Lion Clound network
//                  communication protocol.
//
//  Author        : Tzu Chieh Huang
//  Last Modified : 29th Apr 2020
//

// Include Files
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>

// Project Include Files
#include <lcloud_filesys.h>
#include <cmpsc311_util.h>
#include <lcloud_network.h>
#include <cmpsc311_log.h>

int socket_handle = -1;

//
// Functions
// Function     : create_connection
// Description  : to create the connection
// Outputs      : 0 for true, -1 for failure
int create_connection()
{
    // IF there isn't an open connection already created, three things need to be done
    //    (a) Setup the address
    //    (b) Create the socket
    //    (c) Create the connection

    struct sockaddr_in sockaddr;
	//if the socket has error
    if ((socket_handle = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        logMessage(LOG_OUTPUT_LEVEL, "socket error");
        return -1;
    }
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(LCLOUD_DEFAULT_PORT);
    if (inet_aton(LCLOUD_DEFAULT_IP, &sockaddr.sin_addr) == 0) { // inet_aton return 0 if failure
        logMessage(LOG_OUTPUT_LEVEL, "inet_aton error");
        return -1;
    }
    bzero(&(sockaddr.sin_zero), 8);

    if (connect(socket_handle, (struct sockaddr*)&sockaddr, sizeof(struct sockaddr)) == -1) {
        logMessage(LOG_OUTPUT_LEVEL, "connect error");
        return -1;
    }

    return 0;
}

void extract_lcloud_registers2(LCloudRegisterFrame lcloud_reg, int* b0, int* b1,
    int* c0, int* c1, int* c2, int* d0, int* d1)
{
    // address end in 0xffffffff
    // if each register has memory in it, extract it, returning to the original address
    if (b0)
        *b0 = (int)((lcloud_reg >> 60) & 0xf);
    if (b1)
        *b1 = (int)((lcloud_reg >> 56) & 0xf);
    if (c0)
        *c0 = (int)((lcloud_reg >> 48) & 0xff);
    if (c1)
        *c1 = (int)((lcloud_reg >> 40) & 0xff);
    if (c2)
        *c2 = (int)((lcloud_reg >> 32) & 0xff);
    if (d0)
        *d0 = (int)((lcloud_reg >> 16) & 0xffff);
    if (d1)
        *d1 = (int)((lcloud_reg)&0xffff);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_lcloud_bus_request
// Description  : This the client regstateeration that sends a request to the
//                lion client server.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : reg - the request reqisters for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

LCloudRegisterFrame client_lcloud_bus_request(LCloudRegisterFrame reg, void* buf)
{
    if (socket_handle == -1) {
        if (create_connection() == -1) {
            return -1;
        }
    }

    int opcode, c2;
    extract_lcloud_registers2(reg, NULL, NULL, &opcode, NULL, &c2, NULL, NULL);
    LCloudRegisterFrame network_reg = htonll64(reg);

    // There are four cases to consider when extracting this opcode.
    if (opcode == LC_BLOCK_XFER && c2 == LC_XFER_READ) {
        // CASE 1: read operation (look at the c0 and c2 fields)
        // SEND: (reg) <- Network format : send the register reg to the network
        // after converting the register to 'network format'.
        //
        // RECEIVE: (reg) -> Host format
        //          256 frame (Data read from that frame)

        if (send(socket_handle, &network_reg, sizeof(network_reg), 0) == -1) {
            logMessage(LOG_OUTPUT_LEVEL, "send error");
            return -1;
        }

        if (recv(socket_handle, &network_reg, sizeof(network_reg), 0) == -1) {
            logMessage(LOG_OUTPUT_LEVEL, "recv error");
            return -1;
        }
        if (recv(socket_handle, buf, LC_DEVICE_BLOCK_SIZE, 0) == -1) {
            logMessage(LOG_OUTPUT_LEVEL, "recv error");
            return -1;
        }
    } else if (opcode == LC_BLOCK_XFER && c2 == LC_XFER_WRITE) {
        // CASE 2: write operation (look at the c0 and c2 fields)
        // SEND: (reg) <- Network format : send the register reg to the network
        // after converting the register to 'network format'.
        //       buf 256 (Data to write to that frame)
        //
        // RECEIVE: (reg) -> Host format

        if (send(socket_handle, &network_reg, sizeof(network_reg), 0) == -1) {
            logMessage(LOG_OUTPUT_LEVEL, "send error");
            return -1;
        }
        if (send(socket_handle, buf, LC_DEVICE_BLOCK_SIZE, 0) == -1) {
            logMessage(LOG_OUTPUT_LEVEL, "send error");
            return -1;
        }
        if (recv(socket_handle, &network_reg, sizeof(network_reg), 0) == -1) {
            logMessage(LOG_OUTPUT_LEVEL, "recv error");
            return -1;
        }
    } else if (opcode == LC_POWER_OFF) {
        // CASE 3: power off operation
        // SEND: (reg) <- Network format : send the register reg to the network
        // after converting the register to 'network format'.
        //
        // RECEIVE: (reg) -> Host format
        //
        // Close the socket when finished : reset socket_handle to initial value of -1.
        // close(socket_handle)

        if (send(socket_handle, &network_reg, sizeof(network_reg), 0) == -1) {
            logMessage(LOG_OUTPUT_LEVEL, "send error");
            return -1;
        }
        if (recv(socket_handle, &network_reg, sizeof(network_reg), 0) == -1) {
            logMessage(LOG_OUTPUT_LEVEL, "recv error");
            return -1;
        }

        close(socket_handle);
        socket_handle = -1;
    } else {
        // CASE 4: Other operations (probes, ...)
        // SEND: (reg) <- Network format : send the register reg to the network
        // after converting the register to 'network format'.
        //
        // RECEIVE: (reg) -> Host format

        if (send(socket_handle, &network_reg, sizeof(network_reg), 0) == -1) {
            logMessage(LOG_OUTPUT_LEVEL, "send error");
            return -1;
        }
        if (recv(socket_handle, &network_reg, sizeof(network_reg), 0) == -1) {
            logMessage(LOG_OUTPUT_LEVEL, "recv error");
            return -1;
        }
    }

    return ntohll64(network_reg);
}
