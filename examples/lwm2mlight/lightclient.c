/*******************************************************************************
 *
 * Copyright (c) 2013, 2014, 2015 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    Benjamin Cabé - Please refer to git log
 *    Fabien Fleutot - Please refer to git log
 *    Simon Bernard - Please refer to git log
 *    Julien Vermillard - Please refer to git log
 *    Axel Lorente - Please refer to git log
 *    Toby Jaffey - Please refer to git log
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    Christian Renz - Please refer to git log
 *
 *******************************************************************************/

/*
 Copyright (c) 2013, 2014 Intel Corporation

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
     * Neither the name of Intel Corporation nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 David Navarro <david.navarro@intel.com>
 Bosch Software Innovations GmbH - Please refer to git log

*/

#include "liblwm2m.h"
#include "dtlsconnection.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

extern lwm2m_object_t * get_object_device(void);
extern void free_object_device(lwm2m_object_t * objectP);

extern lwm2m_object_t * get_server_object(int serverId,
                                   const char* binding,
                                   int lifetime,
                                   bool storing);
extern void clean_server_object(lwm2m_object_t * object);

extern lwm2m_object_t * get_security_object(int serverId,
                                     const char* serverUri,
                                     char * bsPskId,
                                     char * psk,
                                     uint16_t pskLen,
                                     bool isBootstrap);
extern void clean_security_object(lwm2m_object_t * objectP);

extern char * get_server_uri(lwm2m_object_t * objectP, uint16_t secObjInstID);

extern lwm2m_object_t * get_test_object(void);
extern void free_test_object(lwm2m_object_t * object);
extern void set_lwm2m_signalpid(pid_t tgt_pid);
extern lwm2m_uri_t *get_target_uri(void);

/* =================================================================== */
/* Server Setting. */

#define LESHAN_SERVER
// #define DOCOMO_SERVER
// #define DOCOMO_SERVER_NOBOOTSTRAP

#ifdef LESHAN_SERVER
  #define TEST_SERVER_NAME "leshan.eclipseprojects.io"
  #define TEST_SERVER_PORT "5684"
  #define TEST_SERVER_ID   "test-spresense"
  #define TEST_SERVER_KEY  "42444b2d5453"
  char psk_key[] =  {0x42, 0x44, 0x4b, 0x2d, 0x54, 0x53};
  int psk_len = 6;
  #define TEST_CLIENT_NAME  "spresense"
#endif

#ifdef DOCOMO_SERVER
  #define TEST_SERVER_NAME "bs.lwm2m.stage.docomodev.net"
  #define TEST_SERVER_PORT "5684"
  #define TEST_SERVER_ID   "xaEGG5XmDWptsUQmi0Og3kwHWgl6jzrY"
  #define TEST_SERVER_KEY  "634943486c386c6579503274644e4654"
  #define TEST_CLIENT_NAME  "351521100130855"
#endif

#ifdef DOCOMO_SERVER_NOBOOTSTRAP
  #define TEST_SERVER_NAME "lwm2m.stage.docomodev.net"
  #define TEST_SERVER_PORT "5684"
  #define TEST_SERVER_ID   "DEVICE_TEST"
  #define TEST_SERVER_KEY  "3132333435363738"
  char psk_key[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};
  int psk_len = 8;
  #define TEST_CLIENT_NAME  "351521100130855"
#endif

/* =================================================================== */


#define MAX_PACKET_SIZE 1024

int g_reboot = 0;
static int g_quit = 0;

typedef struct
{
    lwm2m_object_t * securityObjP;
    int sock;
    dtls_connection_t * connList;
    lwm2m_context_t * lwm2mH;
    int addressFamily;
} client_data_t;


void handle_sigusr1(int signum)
{
  printf("Signal detected\n");
}


void * lwm2m_connect_server(uint16_t secObjInstID,
                            void * userData)
{
  client_data_t * dataP;
  lwm2m_list_t * instance;
  dtls_connection_t * newConnP = NULL;
  dataP = (client_data_t *)userData;
  lwm2m_object_t  * securityObj = dataP->securityObjP;

  instance = LWM2M_LIST_FIND(dataP->securityObjP->instanceList, secObjInstID);
  if (instance == NULL) return NULL;


  newConnP = connection_create(dataP->connList, dataP->sock, securityObj, instance->id, dataP->lwm2mH, dataP->addressFamily);
  if (newConnP == NULL)
  {
      fprintf(stderr, "Connection creation failed.\n");
      return NULL;
  }

  dataP->connList = newConnP;
  return (void *)newConnP;
}

void lwm2m_close_connection(void * sessionH,
                            void * userData)
{
    client_data_t * app_data;
    dtls_connection_t * targetP;

    app_data = (client_data_t *)userData;
    targetP = (dtls_connection_t *)sessionH;

    if (targetP == app_data->connList)
    {
        app_data->connList = targetP->next;
        lwm2m_free(targetP);
    }
    else
    {
        dtls_connection_t * parentP;

        parentP = app_data->connList;
        while (parentP != NULL && parentP->next != targetP)
        {
            parentP = parentP->next;
        }
        if (parentP != NULL)
        {
            parentP->next = targetP->next;
            lwm2m_free(targetP);
        }
    }
}


void print_state(lwm2m_context_t * lwm2mH)
{
    lwm2m_server_t * targetP;

    fprintf(stderr, "State: ");
    switch(lwm2mH->state)
    {
    case STATE_INITIAL:
        fprintf(stderr, "STATE_INITIAL");
        break;
    case STATE_BOOTSTRAP_REQUIRED:
        fprintf(stderr, "STATE_BOOTSTRAP_REQUIRED");
        break;
    case STATE_BOOTSTRAPPING:
        fprintf(stderr, "STATE_BOOTSTRAPPING");
        break;
    case STATE_REGISTER_REQUIRED:
        fprintf(stderr, "STATE_REGISTER_REQUIRED");
        break;
    case STATE_REGISTERING:
        fprintf(stderr, "STATE_REGISTERING");
        break;
    case STATE_READY:
        fprintf(stderr, "STATE_READY");
        break;
    default:
        fprintf(stderr, "Unknown !");
        break;
    }
    fprintf(stderr, "\r\n");

    targetP = lwm2mH->bootstrapServerList;

    if (lwm2mH->bootstrapServerList == NULL)
    {
        fprintf(stderr, "No Bootstrap Server.\r\n");
    }
    else
    {
        fprintf(stderr, "Bootstrap Servers:\r\n");
        for (targetP = lwm2mH->bootstrapServerList ; targetP != NULL ; targetP = targetP->next)
        {
            fprintf(stderr, " - Security Object ID %d", targetP->secObjInstID);
            fprintf(stderr, "\tHold Off Time: %lu s", (unsigned long)targetP->lifetime);
            fprintf(stderr, "\tstatus: ");
            switch(targetP->status)
            {
            case STATE_DEREGISTERED:
                fprintf(stderr, "DEREGISTERED\r\n");
                break;
            case STATE_BS_HOLD_OFF:
                fprintf(stderr, "CLIENT HOLD OFF\r\n");
                break;
            case STATE_BS_INITIATED:
                fprintf(stderr, "BOOTSTRAP INITIATED\r\n");
                break;
            case STATE_BS_PENDING:
                fprintf(stderr, "BOOTSTRAP PENDING\r\n");
                break;
            case STATE_BS_FINISHED:
                fprintf(stderr, "BOOTSTRAP FINISHED\r\n");
                break;
            case STATE_BS_FAILED:
                fprintf(stderr, "BOOTSTRAP FAILED\r\n");
                break;
            default:
                fprintf(stderr, "INVALID (%d)\r\n", (int)targetP->status);
            }
            fprintf(stderr, "\r\n");
        }
    }

    if (lwm2mH->serverList == NULL)
    {
        fprintf(stderr, "No LWM2M Server.\r\n");
    }
    else
    {
        fprintf(stderr, "LWM2M Servers:\r\n");
        for (targetP = lwm2mH->serverList ; targetP != NULL ; targetP = targetP->next)
        {
            fprintf(stderr, " - Server ID %d", targetP->shortID);
            fprintf(stderr, "\tstatus: ");
            switch(targetP->status)
            {
            case STATE_DEREGISTERED:
                fprintf(stderr, "DEREGISTERED\r\n");
                break;
            case STATE_REG_PENDING:
                fprintf(stderr, "REGISTRATION PENDING\r\n");
                break;
            case STATE_REGISTERED:
                fprintf(stderr, "REGISTERED\tlocation: \"%s\"\tLifetime: %lus\r\n", targetP->location, (unsigned long)targetP->lifetime);
                break;
            case STATE_REG_UPDATE_PENDING:
                fprintf(stderr, "REGISTRATION UPDATE PENDING\r\n");
                break;
            case STATE_REG_UPDATE_NEEDED:
                fprintf(stderr, "REGISTRATION UPDATE REQUIRED\r\n");
                break;
            case STATE_DEREG_PENDING:
                fprintf(stderr, "DEREGISTRATION PENDING\r\n");
                break;
            case STATE_REG_FAILED:
                fprintf(stderr, "REGISTRATION FAILED\r\n");
                break;
            default:
                fprintf(stderr, "INVALID (%d)\r\n", (int)targetP->status);
            }
            fprintf(stderr, "\r\n");
        }
    }
}

#define OBJ_COUNT 4


int lightclient_main(void)
{
    client_data_t data;
    lwm2m_context_t * lwm2mH = NULL;
    lwm2m_object_t * objArray[OBJ_COUNT];

    const char * localPort = "56830";

    int result;

    memset(&data, 0, sizeof(client_data_t));

    data.addressFamily = AF_INET;


    /*
     *This call an internal function that create an IPv6 socket on the port 5683.
     */
    fprintf(stderr, "Trying to bind LWM2M Client to port %s\r\n", localPort);
    data.sock = create_socket(localPort, data.addressFamily);
    if (data.sock < 0)
    {
        fprintf(stderr, "Failed to open socket: %d %s\r\n", errno, strerror(errno));
        return -1;
    }

    printf("Use DTLS : %s:%s, %s, %s\n", TEST_SERVER_NAME, TEST_SERVER_PORT, TEST_SERVER_ID, TEST_SERVER_KEY);

    char serverUri[50];
    int serverId = 123;
    sprintf (serverUri, "coaps://%s:%s", TEST_SERVER_NAME, TEST_SERVER_PORT);

    /*
     * Now the main function fill an array with each object, this list will be later passed to liblwm2m.
     * Those functions are located in their respective object file.
     */
    // objArray[0] = get_security_object();
    objArray[0] = get_security_object(serverId, serverUri, TEST_SERVER_ID, psk_key, psk_len, false);
    if (NULL == objArray[0])
    {
        fprintf(stderr, "Failed to create security object\r\n");
        return -1;
    }
    data.securityObjP = objArray[0];

    // objArray[1] = get_server_object();
    objArray[1] = get_server_object(serverId, "U", 300, false);
    if (NULL == objArray[1])
    {
        fprintf(stderr, "Failed to create server object\r\n");
        return -1;
    }

    objArray[2] = get_object_device();
    if (NULL == objArray[2])
    {
        fprintf(stderr, "Failed to create Device object\r\n");
        return -1;
    }

    objArray[3] = get_test_object();
    if (NULL == objArray[3])
    {
        fprintf(stderr, "Failed to create Test object\r\n");
        return -1;
    }

    /*
     * The liblwm2m library is now initialized with the functions that will be in
     * charge of communication
     */
    lwm2mH = lwm2m_init(&data);
    if (NULL == lwm2mH)
    {
        fprintf(stderr, "lwm2m_init() failed\r\n");
        return -1;
    }
    data.lwm2mH = lwm2mH;

    /*
     * We configure the liblwm2m library with the name of the client - which shall be unique for each client -
     * the number of objects we will be passing through and the objects array
     */
    result = lwm2m_configure(lwm2mH, TEST_CLIENT_NAME, NULL, NULL, OBJ_COUNT, objArray);
    if (result != 0)
    {
        fprintf(stderr, "lwm2m_configure() failed: 0x%X\r\n", result);
        return -1;
    }

    /*
     * We catch Ctrl-C signal for a clean exit
     */
    signal(SIGUSR1, handle_sigusr1);
    set_lwm2m_signalpid(getpid());
    printf("Set signal pid=%d\n", getpid());

    fprintf(stdout, "LWM2M Client \"%s\" started on port %s.\r\nUse Ctrl-C to exit.\r\n\n", TEST_CLIENT_NAME, localPort);

    /*
     * We now enter in a while loop that will handle the communications from the server
     */
    while (0 == g_quit)
    {
        struct timeval tv;
        fd_set readfds;

        tv.tv_sec = 60;
        tv.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(data.sock, &readfds);

        print_state(lwm2mH);

        /*
         * This function does two things:
         *  - first it does the work needed by liblwm2m (eg. (re)sending some packets).
         *  - Secondly it adjusts the timeout value (default 60s) depending on the state of the transaction
         *    (eg. retransmission) and the time before the next operation
         */
        result = lwm2m_step(lwm2mH, &(tv.tv_sec));
        if (result != 0)
        {
            fprintf(stderr, "lwm2m_step() failed: 0x%X\r\n", result);
            return -1;
        }

        /*
         * This part wait for an event on the socket until "tv" timed out (set
         * with the precedent function)
         */
        result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

        if (result < 0)
        {
            if (errno == EINTR)
            {
              printf("Update is occured\n");
              lwm2m_resource_value_changed(lwm2mH, get_target_uri());
            }
            else
            {
              fprintf(stderr, "Error in select(): %d %s\r\n", errno, strerror(errno));
            }
        }
        else if (result > 0)
        {
            uint8_t buffer[MAX_PACKET_SIZE];
            int numBytes;

            /*
             * If an event happens on the socket
             */
            if (FD_ISSET(data.sock, &readfds))
            {
                struct sockaddr_storage addr;
                socklen_t addrLen;

                addrLen = sizeof(addr);

                /*
                 * We retrieve the data received
                 */
                numBytes = recvfrom(data.sock, buffer, MAX_PACKET_SIZE, 0, (struct sockaddr *)&addr, &addrLen);

                if (0 > numBytes)
                {
                    fprintf(stderr, "Error in recvfrom(): %d %s\r\n", errno, strerror(errno));
                }
                else if (0 < numBytes)
                {
                    char s[INET6_ADDRSTRLEN];
                    in_port_t port = 0;

                    dtls_connection_t * connP;

                    /* Get connected address */{
                        struct sockaddr_in *saddr = (struct sockaddr_in *)&addr;
                        inet_ntop(saddr->sin_family, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
                        port = saddr->sin_port;
                    }
                    fprintf(stderr, "%d bytes received from [%s]:%hu\r\n", numBytes, s, ntohs(port));

                    connP = connection_find(data.connList, &addr, addrLen);
                    if (connP != NULL)
                    {
                        /*
                         * Let liblwm2m respond to the query depending on the context
                         */
                        // lwm2m_handle_packet(lwm2mH, buffer, numBytes, connP);
                        int result2 = connection_handle_packet(connP, buffer, numBytes);
                        if (0 != result2)
                        {
                             printf("error handling message %d\n",result2);
                        }
                    }
                    else
                    {
                        /*
                         * This packet comes from an unknown peer
                         */
                        fprintf(stderr, "received bytes ignored!\r\n");
                    }
                }
            }
        }
    }

    /*
     * Finally when the loop is left, we unregister our client from it
     */
    lwm2m_close(lwm2mH);
    close(data.sock);
    connection_free(data.connList);

    clean_security_object(objArray[0]);
    clean_server_object(objArray[1]);
    free_object_device(objArray[2]);
    free_test_object(objArray[3]);

    fprintf(stdout, "\r\n\n");

    return 0;
}
