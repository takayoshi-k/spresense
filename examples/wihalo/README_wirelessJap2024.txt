Architecture of this demo app
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  SPRESENSE                         PC
  +--------+            cam_client            Browser
  |        |            +--------+            +-----+
  |Get Cam |            |        |            |     |
  | Send it|-Halow Net->|Rcv Cam |            |     |
  |        |            |HTTP Svr|-local net->|     |---> Display
  +--------+            |        |            +-----+
                        +--------+

Required Devices
~~~~~~~~~~~~~~~~

 * Spresense Main board
 * Vizmonet Halow board
 * Antenna
 * Halow Router
 * Linux PC

How to Build
~~~~~~~~~~~~

  * Spresense side
    $ tools/config.py examples/halow_demo_wirelessjap_2024
    $ make -j

  * Host side
    $ cd examples/wihalo/host
    $ make

How to Execute
~~~~~~~~~~~~~~

  * Spresense side
    Just start wihalo app
    nsh> wihalo

    On the first bootup, this app create a file /mnt/spif/wihalo.cfg.
    You should modify for fitting your environment.
    More details, you can see initialize_configuration() function in
    wihalo.c.

  * PC Side
    $ cd examples/wihalow/host
    $ ./cam_client

    Wait for establishing the connection between cam_client and Spresense.
    After that open a Browser and connect to localhost:8080


