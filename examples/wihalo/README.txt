- How to build

  In sdk/ directory
  $ ./tools/config.py examples/wihalo
  $ make -j

- How to flash

  $ ./tools/flash.sh -c <port name> nuttx.spk

  And you should flash spresense firmware downloaded from 
    https://developer.sony.com/file/download/download-spresense-firmware-v3-1-0

- Hardware configurations

  Two Spresense main boards
  Two WiHalow boards
  One Spresense camera boards.
  One LCD Display of ili9341 connected on main board like https://www.switch-science.com/products/7982

- How to execute demo sending camera image

  Flash above nuttx.spk in 2 boards.
  Boot up both.
  execute wihalo app on both boards.
    nsh> wihalo

  Follow the instruction in section 2.4 and 2.5 in the manual "Wi-Fi HaLow Module User Guide-Ver1.pdf"

  Server serving Camera image.
  Client receiving camera image from the server and display it on LCD.

