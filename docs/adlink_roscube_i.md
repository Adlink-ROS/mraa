ADLINK ROScube-I x86
============

* ADLINK ROScube-I x86

  ROS 2-enabled robotic controller based on Intel® Core™ processors

  For more detail, you can refer to the following link:

  https://www.adlinktech.com/Products/ROS2_Solution/ROS2_Controller/ROScube-I


## Pin Mapping 


| Linux GPIO (/sys/class/gpio) | Function  | MRAA number | MRAA number | Function  | Linux GPIO (/sys/class/gpio) |
| :--------------------------: | :-------: | :---------: | :---------: | :-------: | :--------------------------: |
|             220              |  CN_DI0   |      1      |      2      |  CN_DI1   |            221               |
|             222              |  CN_DI2   |      3      |      4      |  CN_DI3   |            223               |
|             224              |  CN_DI4   |      5      |      6      |  CN_DI5   |            225               |
|             226              |  CN_DI6   |      7      |      8      |  CN_DI7   |            227               |
|                              |    GND    |      9      |     10      |CON_PORT0_CAN-L|                          |
|                              |CON_PORT1_CAN-L|  11     |     12      |GPICON_I2C0_SDAO5|                        |
|                              |CON_I2C0_SCL|     13     |     14      |P_+5V_S_CN|                               |
|                              |P_+3V3_S_CN|     15      |     16      |CON_I2C1_SDA|                             |
|                              |CON_I2C1_SCL|     17     |     18      |    GND    |                              |
|                              |    GND    |     19      |     20      |    GND    |                              |
|                              |    GND    |     21      |     22      |    GND    |                              |
|                              |    GND    |     23      |     24      |    GND    |                              |
|                              |    GND    |     25      |     26      |    GND    |                              |
|                              |    GND    |     27      |     28      |    GND    |                              |
|                              |    GND    |     29      |     30      |    GND    |                              |
|                              |    GND    |     31      |     32      |    GND    |                              |
|                              |    GND    |     33      |     34      |  CN_DO0   |            253               |
|             254              |  CN_DO1   |     35      |     36      |  CN_DO2   |            255               |
|             256              |  CN_DO3   |     37      |     38      |  CN_DO4   |            257               |
|             258              |  CN_DO5   |     39      |     40      |  CN_DO6   |            259               |
|             260              |  CN_DO7   |     41      |     42      |    GND    |                              |
|                            |CON_PORT0_CAN-H|   43      |     44    |CON_PORT1_CAN-H|                            |
|                              |   GND     |     45      |     46      |  unused   |                              |
|                              |   GND     |     47      |     48      |  unused   |                              |
|                              |   GND     |     49      |     50      |    GND    |                              |

* ROScube-I supports the following functions in MRAA currently:
  * GPIO
  * UART
  * LED
  * I2C
