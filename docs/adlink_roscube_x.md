ADLINK ROScube-X ARM
============

* ADLINK ROScube-X ARM

  ROS 2-enabled robotic controller based on NVIDIA® Jetson AGX Xavier™ module

  For more detail, you can refer to the following link:

  https://www.adlinktech.com/Products/ROS2_Solution/ROS2_Controller/ROScube-X


## Pin Mapping 


| Linux GPIO (/sys/class/gpio) | Function  | MRAA number | MRAA number | Function  | Linux GPIO (/sys/class/gpio) |
| :--------------------------: | :-------: | :---------: | :---------: | :-------: | :--------------------------: |
|                              |ADC1_isolate|     1      |      2      |ADC2_isolate|                              |
|                              | Not Used  |      3      |      4      | Not Used  |                              |
|             216              |   GPIO0   |      5      |      6      |   GPIO1   |             217              |
|             218              |   GPIO2   |      7      |      8      |   GPIO3   |             219              |
|             220              |   GPIO4   |      9      |     10      |   GPIO5   |             221              |
|             222              |   GPIO6   |     11      |     12      |   GPIO7   |             223              |
|             224              |   GPIO8   |     13      |     14      |   GPIO9   |             225              |
|             226              |   GPIO10  |     15      |     16      |   GPIO11  |             227              |
|             228              |   GPIO12  |     17      |     18      |    GND    |                              |
|                              | Not Used  |     19      |     20      | Not Used  |                              |
|                              | Not Used  |     21      |     22      |   PWM     |                              |
|                              | SPI0_CLK  |     23      |     24      |  SPI0_CS  |                              |
|                              | SPI0_MISO |     25      |     26      | SPI0_MOSI |                              |
|             229              |   GPIO13  |     27      |     28      |   GPIO14  |             230              |
|             231              |   GPIO15  |     29      |     30      |   GPIO16  |             233              |
|             234              |   GPIO17  |     31      |     32      |   GPIO18  |             235              |
|             236              |   GPIO19  |     33      |     34      |    GND    |                              |
|                              | Not Used  |     35      |     36      | Not Used  |                              |
|                              | Not Used  |     37      |     38      |   CAN_RX  |                              |
|                              |   CAN_TX  |     39      |     40      |   CAN_H   |                              |
|                              |   CAN_L   |     41      |     42      |  I2C_CLK  |                              |
|                              |  I2C_DATA |     43      |     44      |   5V DC   |                              |
|                              |   5V DC   |     45      |     46      | 3.3V DC   |                              |
|                              |   GND     |     47      |     48      |    GND    |                              |
|                              |   GND     |     49      |     50      |    GND    |                              |