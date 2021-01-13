ADLINK ROScube-Pico-NX ARM
============

* ADLINK ROScube-Pico-NX ARM

  ROS 2-enabled robotic controller based on NVIDIA® Jetson Xavier™ NX module

  For more detail, you can refer to the following link:

  https://www.adlinktech.com/Products/ROS2_Solution/


## Pin Mapping 


| Linux GPIO (/sys/class/gpio) |    Function    | MRAA number | MRAA number |    Function    | Linux GPIO (/sys/class/gpio) |
| :--------------------------: | :------------: | :---------: | :---------: | :------------: | :--------------------------: |
|                              |    PWR_BTN#    |       1     |      2      |      GND       |                              |
|                              | FORCE_ROCOVERY |       3     |      4      |      GND       |                              |
|                              |    SYS_RST     |       5     |      6      |      GND       |                              |
|                              |DEBUG_CONSOLE_TX|       7     |      8      |DEBUG_CONSOLE_RX|                              |
|                              |      GND       |       9     |     10      |    I2C0_SCL    |                              |
|                              |    I2C0_SDA    |      11     |     12      |    SPI0_CS1    |                              |
|                              |    SPI0_CS0    |      13     |     14      |      GND       |                              |
|                              |    UART0_RX    |      15     |     16      |    UART0_TX    |                              |
|                              |      GND       |      17     |     18      |       V5       |                              |
|                              |       V5       |      19     |     20      |      GND       |                              |
|                              |     CAN_L      |      21     |     22      |     CAN_H      |                              |
|                              |      GND       |      23     |     24      |      GPIO7     |             238              |
|             237              |      GPIO6     |      25     |     26      |      GPIO5     |             236              |
|             235              |      GPIO4     |      27     |     28      |      GPIO3     |             234              |
|                              |      GND       |      29     |     30      |     SPI_CLK    |                              |
|                              |     MISO0      |      31     |     32      |      MOSI0     |                              |
|                              |     GPIO19     |      33     |     34      |      GND       |                              |
|                              |    I2C1_SCL    |      35     |     36      |    I2C1_SDA    |                              |
|                              |      3v3       |      37     |             |                |                              |


## Serial    port mapping
 |    Serial port   | MRAA number | Bash Path  |
 |:----------------:|:-----------:|:----------:|
 |   UART on DB37   |      0      |/dev/ttyTHS1|
