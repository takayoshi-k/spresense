
#ifndef __wihalo_api__
#define __wihalo_api__


// Set WH_APP_MODE to 0 for standalone mode and 1 for application mode
// In application mode, the whapp.c will be used

#define WH_APP_MODE     1        




/*
    API specifications for using the NRC-7292 MCU/WiFi modem using ATCMD interface.
*/

typedef enum  {
    WH_SOCK_TCP,
    WH_SOCK_UDP
} WH_SOCK_TYPE;

typedef enum  {
    WH_ACCESS_POINT,
    WH_STATION
} WH_WIFI_MODE;


// Temp function to read user input, only included for 
// sample application
char* readstdin(char *buf, int max);



/*
    Function wh_initialize 
    Should be called at startup to initialize the modem and the SPI hardware
*/
int wh_initialize();

/*
    Function wh_start
    Specify either Access point or Station to start in the respective mode
*/
int wh_start(WH_WIFI_MODE mode);

/*
    Function wh_stop
    Stops the SPI processes and receive/monitor threads running in wihalo library
*/
void wh_stop();

/*
    Function wh_reset
    Resets the NRC modem. Can be used to restart the device either as
    a station or an access point
*/
void wh_reset();

/*
    Function wh_post_command
    WiHalo library supports standalone operation. In a typical application, the 
    commands supported in teh standalone mode are also supported in the main 
    application. This is used mainly for debugging purposes or sending the ATCMD's
    directly to the NRC modem. Requries a thourough understanding of the ATCMD set.
    For example, 'help' command displays the supported standalone mode commands.
*/
int wh_post_command(char *cmd) ;

// Set commands, for details please refer to the NRC user manuals

/*
    Function wh_set_ssid
    Used to set the SSID name
*/
int wh_set_ssid(char *ssid);

/*
    Function wh_set_frequency
    Used to select the channel and the bandwidths supported by the NRC modem.
    For more details, refer to the NRC documentation.
    Default : 918.5 Mhz @ 1 MHZ bandwidth 
*/
int wh_set_frequency(float freq);

/*
    Function wh_set_country
    Used to set the country of operation.
    Refer to NRC documentation for more details.
*/
int wh_set_country(char *country);

/*
    Function wh_set_ip_params
    Used to configure the IP address, Mask and the default gateway.
    The default values are 192.168.1.1, 255.255.255.0 and 192.168.1.1
    Should be used on an access point. Can be used on the station if static addressing
    is used and DHCP is not enabled
*/
int wh_set_ip_params(char *ip, char *mask, char *gw);

/*
    Function wh_set_dhcp
    Used to enable/disable DHCP
*/
int wh_set_dhcp(int dhcp);

/*
    Function wh_set_security
    Used to configure the security functionality.
    For details refer to NRC documentation. The default security is none
*/
int wh_set_security(char *security);

/*
    Function wh_set_password
    Used to configure the security functionality and the associate password.
    Parameter is ignored when the operation is set to 'open' mode
*/
int wh_set_password(char *password);

/*
    Arguments
    sta_ap =    0 for station
                1 for Access point

    Returns
    0 for success
    -1 for improper values

*/
int wh_set_mode(int sta_ap);

/* Set receive LOG level, level 1 required for UDP transfers */
void wh_set_loglevel(int level);

/*
    Returns 1 for connected
            0 for disconnected
*/
int wh_get_state();


/*
    Input type = WH_SOCK_TYPE
          port - unsigned 16 bit port number

    Returns
        new socket id (int) if successful  OR
        -1 if failed
*/
int wh_socket_listen(WH_SOCK_TYPE type, uint16_t port);


/*
    Function wh_open_socket
    Used to create a socket descriptor on the NRC modem.
    When the call fails, returns -1, probably because of the wrong parameters or
    due to an already open socket with overlapping parameters.
*/
int wh_open_socket(WH_SOCK_TYPE type, uint16_t port, char *optional_remote_or_null);

/*
    Function wh_close_socket
    Used to close/terminate an open socket descriptor.
    If -1 is passed as socked id, all sockets are closed
*/
int wh_close_socket(int sckid);


/*
    wh_set_passmode
    Used to enable/disable passthrough mode for streaming
*/
int wh_set_passmode(int);

/*
    wh_end_passmode
    Used to terminate the passthroughmode
*/
int wh_end_passmode();

/*
    Function wh_start_passmode
    Used to start streaming on a TCP socket
    'id' is the socket id
    Note: All data is directed towards the conencted socket with id. To send on another 
    connection or to transfer any other data, passmode should be terminated by calling
    after a sleep duration of 2 seconds, wh_end_passmode
*/
int wh_start_passmode(int id);

/* Event types */

#define EVENT_TYPE_CONNECT  4
#define EVENT_TYPE_CLOSED   5
#define EVENT_TYPE_RCVERR   10
#define EVENT_TYPE_SENDIDLE 6



/*
Callack function prototype
    data_evt    - Indicates a data received (0) or an event on the associated socket (1)
    sckid   - The associated socket id
    event   - The event type when data_evt is 1 (EVENT_TYPE)
            0 - data
            1 - other event
    remote  - Pointer to ip address of the remote (may be null)
    port    - Associated port
    data    - Pointer to data received if event==0
    len     - Length of the data if available
    evt_id  - Id of the error/event
                0 - No error
                1 - Connected to remote
                2 - Remote closed a connection
                3 - Receive error (Sock should be closed)

    Should return 
        0 - Not processed
        1 - Processed
*/
typedef int (*socket_event_callback) (int data_evt, int sckid, int event, char *remote, uint16_t port, uint8_t *data, int len, int err_id );

/*
    Function wh_register_callback
    Used to enable a callback on socket events and data received events.
    NOTE: The application should not attempt to send socket or other commands 
    to NRC modem in the callback functions. Use of semaphores or other queue mechanisms 
    is recommended to initiate responses to the received events
*/
void wh_register_callback(socket_event_callback);

/*
    Function wh_unregister_callback
    Will cancel any previously associate callback functions for socket or data 
    received events 
*/
void wh_unregister_callback(void);

/*
    Function wh_send_tcp_data
    Used to send user/application data on a connected socket, using atcmd 
*/
int wh_send_tcp_data(int sckid, uint8_t *data, int datalen);

/*
    Function wh_send_tcp_passmode_data
    Used to send data to remote connected socket in passthrough mode
    Prior to sending the data, the passthrough mode has to be initialized using
    the api call wh_start_passmode(socket_id);
*/
int wh_send_tcp_passmode_data(uint8_t *data, int datalen);


/*
    Function wh_send_udp_data
    Used to send data on a previously opened UDP socket descriptor.
    The remote address and port have to be passed in the case of UDP transfers.
*/
int wh_send_udp_data(int sckid, char *remote, uint16_t port, uint8_t *data, int datalen);


/*
    Function wh_get_local_ip
    Used to retrieve the local IPV4
    Pass a string buffer of at least 15 bytes to get the IPV4.
    The function fails with return of -1, if the IP address cannot be retrieved or if the 
    device is not connected to the network
*/
int wh_get_local_ip_params(char *ip, char *mask, char *gw);



#define WHLOG_ENABLE    1
#define WHINFO_ENABLE   1


#if WHLOG_ENABLE
#define WHLOG(...) printf(__VA_ARGS__)
#else
#define WHLOG(...) void(__VA_ARGS__)
#endif

#if WHINFO_ENABLE
#define WHINFO(...) printf(__VA_ARGS__)
#else
#define WHINFO(...) void(__VA_ARGS__)
#endif



#endif
