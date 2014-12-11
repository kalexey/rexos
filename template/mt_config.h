/*
    RExOS - embedded RTOS
    Copyright (c) 2011-2014, Alexey Kramarenko
    All rights reserved.
*/

#ifndef MT_CONFIG_H
#define MT_CONFIG_H

//----------------------- pin definition -------------------------------------------
//port for data lines D0..D7
#define DATA_PORT                       GPIO_PORT_A
//pin numbers mask for data lines. Don't change.
#define DATA_MASK                       0xff

//port for address, chip select and direction lines
#define ADDSET_PORT                     GPIO_PORT_B
//chip select pin number
#define MT_CS1                          (1 << 7)
#define MT_CS2                          (1 << 6)
//read/write pin number
#define MT_RW                           (1 << 4)
//data/command pin number
#define MT_A                            (1 << 3)

//backlight pin
#define MT_BACKLIGHT                    B9
//reset pin
#define MT_RESET                        B5
//strobe pin
#define MT_STROBE                       A15

//------------------------------ timeouts ------------------------------------------
//all CLKS time. Refer to datasheet for more details
//Address hold time
#define TAS                             1
//Data read prepare time
#define TDDR                            17
//Delay between commands
#define TW                              256
//Reset time (max)
#define TR                              320
//Reset impulse time (max)
#define TRI                             32
//------------------------------ general ---------------------------------------------
//pixel test api
#define MT_TEST                         0

#define X_MIRROR                        1

//------------------------------ process -------------------------------------------
//Use as driver or as library. Driver has some switch latency time and require stack memory. However,
//many processes can use low-level LCD API
#define MT_DRIVER                       0
#define MT_STACK_SIZE                   340
//increase in case when many process use LCD at same time
#define MT_IPC_COUNT                    5

#endif // MT_CONFIG_H
