/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
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
 *    domedambrosio - Please refer to git log
 *    Fabien Fleutot - Please refer to git log
 *    Axel Lorente - Please refer to git log
 *    Achim Kraus, Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    Ville Skyttä - Please refer to git log
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

*/

/*
 * Implements an object for testing purpose
 *
 *                  Multiple
 * Object |  ID   | Instances | Mandatory |
 *  Test  | 32100 |    No     |    No     |
 *
 *  Resources:
 *                           Supported    Multiple
 *  Name           | ID    | Operations | Instances | Mandatory |  Type   | Range | Units |      Description      |
 *  recognized num |  5700 |    RO      |    No     |    Yes    | String  |       |       |                       |
 *
 */

#include "liblwm2m.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <limits.h>

#define DOCOMO_DEMO_OBJECT_ID   (32100)
#define DOCOMO_DEMO_INSTANCE_NO (1)
#define DOCOMO_DEMO_RESOURCE_RECOGNIZED_VALUE (5527)

#if 0
static void prv_output_buffer(uint8_t * buffer,
                              int length)
{
    int i;
    uint8_t array[16];

    i = 0;
    while (i < length)
    {
        int j;
        fprintf(stderr, "  ");

        memcpy(array, buffer+i, 16);

        for (j = 0 ; j < 16 && i+j < length; j++)
        {
            fprintf(stderr, "%02X ", array[j]);
        }
        while (j < 16)
        {
            fprintf(stderr, "   ");
            j++;
        }
        fprintf(stderr, "  ");
        for (j = 0 ; j < 16 && i+j < length; j++)
        {
            if (isprint(array[j]))
                fprintf(stderr, "%c ", array[j]);
            else
                fprintf(stderr, ". ");
        }
        fprintf(stderr, "\n");

        i += 16;
    }
}
#endif


/*
 * Multiple instance objects can use userdata to store data that will be shared between the different instances.
 * The lwm2m_object_t object structure - which represent every object of the liblwm2m as seen in the single instance
 * object - contain a chained list called instanceList with the object specific structure prv_instance_t:
 */
typedef struct _prv_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data (uint8_t test in this case)
     */
    struct _prv_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    char recognition_num[16];
} prv_instance_t;

static prv_instance_t *gLocalInst = NULL;

static uint8_t prv_read(uint16_t instanceId,
                        int * numDataP,
                        lwm2m_data_t ** dataArrayP,
                        lwm2m_object_t * objectP)
{
    prv_instance_t * targetP;
    int i;

    targetP = (prv_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(DOCOMO_DEMO_INSTANCE_NO);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = DOCOMO_DEMO_INSTANCE_NO;

        (*dataArrayP)[0].id = DOCOMO_DEMO_RESOURCE_RECOGNIZED_VALUE ;
    }

    for (i = 0 ; i < *numDataP ; i++)
    {
        switch ((*dataArrayP)[i].id)
        {
        case DOCOMO_DEMO_RESOURCE_RECOGNIZED_VALUE:
            printf("[DOCOMO Demo] send data >>%s<<\n", targetP->recognition_num);
            lwm2m_data_encode_string(targetP->recognition_num, *dataArrayP + i);
            break;
        default:
            return COAP_404_NOT_FOUND;
        }
    }

    return COAP_205_CONTENT;
}

static uint8_t prv_discover(uint16_t instanceId,
                            int * numDataP,
                            lwm2m_data_t ** dataArrayP,
                            lwm2m_object_t * objectP)
{
    int i;

    // is the server asking for the full object ?
    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(DOCOMO_DEMO_INSTANCE_NO);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = DOCOMO_DEMO_INSTANCE_NO;

        (*dataArrayP)[0].id = DOCOMO_DEMO_RESOURCE_RECOGNIZED_VALUE ;
    }
    else
    {
        for (i = 0; i < *numDataP; i++)
        {
            switch ((*dataArrayP)[i].id)
            {
            case DOCOMO_DEMO_RESOURCE_RECOGNIZED_VALUE :
                break;
            default:
                return COAP_404_NOT_FOUND;
            }
        }
    }
    return COAP_205_CONTENT;
}

#if 0
static uint8_t prv_write(uint16_t instanceId,
                         int numData,
                         lwm2m_data_t * dataArray,
                         lwm2m_object_t * objectP)
{
    prv_instance_t * targetP;
    int i;

    targetP = (prv_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    for (i = 0 ; i < numData ; i++)
    {
        switch (dataArray[i].id)
        {
        case 1:
        {
            int64_t value;

            if (1 != lwm2m_data_decode_int(dataArray + i, &value) || value < 0 || value > 0xFF)
            {
                return COAP_400_BAD_REQUEST;
            }
            targetP->test = (uint8_t)value;
        }
        break;
        case 2:
            return COAP_405_METHOD_NOT_ALLOWED;
        case 3:
            if (1 != lwm2m_data_decode_float(dataArray + i, &(targetP->dec)))
            {
                return COAP_400_BAD_REQUEST;
            }
            break;
        case 4:
        {
            int64_t value;

            if (1 != lwm2m_data_decode_int(dataArray + i, &value) || value < INT16_MIN || value > INT16_MAX)
            {
                return COAP_400_BAD_REQUEST;
            }
            targetP->sig = (int16_t)value;
        }
        break;
        default:
            return COAP_404_NOT_FOUND;
        }
    }

    return COAP_204_CHANGED;
}
#endif

#if 0
static uint8_t prv_delete(uint16_t id,
                          lwm2m_object_t * objectP)
{
    prv_instance_t * targetP;

    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, id, (lwm2m_list_t **)&targetP);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    lwm2m_free(targetP);

    return COAP_202_DELETED;
}
#endif

#if 0
static uint8_t prv_create(uint16_t instanceId,
                          int numData,
                          lwm2m_data_t * dataArray,
                          lwm2m_object_t * objectP)
{
    prv_instance_t * targetP;
    uint8_t result;


    targetP = (prv_instance_t *)lwm2m_malloc(sizeof(prv_instance_t));
    if (NULL == targetP) return COAP_500_INTERNAL_SERVER_ERROR;
    memset(targetP, 0, sizeof(prv_instance_t));

    targetP->shortID = instanceId;
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);

    result = prv_write(instanceId, numData, dataArray, objectP);

    if (result != COAP_204_CHANGED)
    {
        (void)prv_delete(instanceId, objectP);
    }
    else
    {
        result = COAP_201_CREATED;
    }

    return result;
}
#endif

#if 0
static uint8_t prv_exec(uint16_t instanceId,
                        uint16_t resourceId,
                        uint8_t * buffer,
                        int length,
                        lwm2m_object_t * objectP)
{

    if (NULL == lwm2m_list_find(objectP->instanceList, instanceId)) return COAP_404_NOT_FOUND;

    switch (resourceId)
    {
    case 1:
        return COAP_405_METHOD_NOT_ALLOWED;
    case 2:
        fprintf(stdout, "\r\n-----------------\r\n"
                        "Execute on %hu/%d/%d\r\n"
                        " Parameter (%d bytes):\r\n",
                        objectP->objID, instanceId, resourceId, length);
        prv_output_buffer((uint8_t*)buffer, length);
        fprintf(stdout, "-----------------\r\n\r\n");
        return COAP_204_CHANGED;
    case 3:
        return COAP_405_METHOD_NOT_ALLOWED;
    default:
        return COAP_404_NOT_FOUND;
    }
}
#endif

static pid_t target_pid = -1;
static lwm2m_uri_t update_uri = {
  .objectId   = DOCOMO_DEMO_OBJECT_ID,
  .instanceId = DOCOMO_DEMO_INSTANCE_NO,
  .resourceId = DOCOMO_DEMO_RESOURCE_RECOGNIZED_VALUE
};

void updateRecognitionValue(int inst_no, char *data)
{
  if( gLocalInst != NULL && target_pid > 0)
    {
      strncpy(gLocalInst->recognition_num, data, 16);
      gLocalInst->recognition_num[15] = '\0';
      kill(target_pid, SIGUSR1);
    }
}

void set_lwm2m_signalpid(pid_t tgt_pid)
{
  target_pid = tgt_pid;
}

lwm2m_uri_t *get_target_uri(void)
{
  return &update_uri;
}

lwm2m_object_t * get_test_object(void)
{
    lwm2m_object_t * testObj;

    testObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != testObj)
    {
        prv_instance_t * targetP;

        memset(testObj, 0, sizeof(lwm2m_object_t));

        testObj->objID = DOCOMO_DEMO_OBJECT_ID;

        /* Create an Instance data */
        targetP = (prv_instance_t *)lwm2m_malloc(sizeof(prv_instance_t));
        if (NULL == targetP) return NULL;
        memset(targetP, 0, sizeof(prv_instance_t));
        targetP->shortID = DOCOMO_DEMO_INSTANCE_NO;
        targetP->recognition_num[0] = 'N';
        targetP->recognition_num[1] = 'o';
        targetP->recognition_num[2] = ' ';
        targetP->recognition_num[3] = 'C';
        targetP->recognition_num[4] = 'a';
        targetP->recognition_num[5] = 'r';
        targetP->recognition_num[6] = 'd';
        targetP->recognition_num[7] = '\0';
        testObj->instanceList = LWM2M_LIST_ADD(testObj->instanceList, targetP);
        gLocalInst = targetP;

        /*
         * From a single instance object, two more functions are available.
         * - The first one (createFunc) create a new instance and filled it with the provided informations. If an ID is
         *   provided a check is done for verifying his disponibility, or a new one is generated.
         * - The other one (deleteFunc) delete an instance by removing it from the instance list (and freeing the memory
         *   allocated to it)
         */
        testObj->readFunc = prv_read;
        // testObj->writeFunc = prv_write;
        // testObj->executeFunc = prv_exec;
        // testObj->createFunc = prv_create;
        // testObj->deleteFunc = prv_delete;
        testObj->discoverFunc = prv_discover;
    }

    return testObj;
}

void free_test_object(lwm2m_object_t * object)
{
    LWM2M_LIST_FREE(object->instanceList);
    if (object->userData != NULL)
    {
        lwm2m_free(object->userData);
        object->userData = NULL;
    }
    lwm2m_free(object);
}
