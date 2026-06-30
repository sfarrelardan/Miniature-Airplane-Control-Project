/******************************************************************************
* Copyright (C) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/
/*
 * helloworld.c: simple test application for the plane project.
 * AUTHOR: Yitao LIN et Farrel Ardan SUSILO
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include <sys/_intsup.h>
#include <sys/types.h>
#include <xil_types.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"

// offset 
#define AXDL_XDATA_ADDR         0x08
#define AXDL_YDATA_ADDR         0x09
#define AXDL_ZDATA_ADDR         0x0a    
#define PWM_XAXIS_OFFSET        0x00    // slave register 0
#define PWM_YAXIS_OFFSET        0x04    // slave register 1


int main()
{   
    /****************************/
    /*      INITIALIZATION      */
    /****************************/
    init_platform();
    printf("Hello EMBEDED BROS INC.\n");
    volatile unsigned int* x_axis = (unsigned int *) (XPAR_PWM_0_BASEADDR + PWM_XAXIS_OFFSET);
    volatile unsigned int* y_axis = (unsigned int *) (XPAR_PWM_0_BASEADDR + PWM_YAXIS_OFFSET);
    *x_axis = 750;  // upright 
    *y_axis = 750;
    msleep(2000);
    printf("Initialization finished.\n");


    /******************************************************************************
    *                         How to use this code ?                              *
    * Uncomment the section you want to use, comment the sections you don't need. *
    * ATTENTION: Only one section can work at a time.                             *
    *******************************************************************************/


    /****************************/
    /*     AUTOPILOT SECTION    */
    /****************************/
    // SPI_ADXL_Init();            // initialize ADXL SPI controller
    // printf("Autopiloting.\n");
    // ADXL362_Init();             // initialize ADXL
    // printf("Accelerometer init done.\n");
    // ADXL362_SetRange();         // set acceleration range to measure
    // char x_a = 0;
    // char y_a = 0;
    

    // while (1) {
    //     while (!ADXL362_IsDataReady())  // wait for TX FIFO to empty
    //         ;

    //     x_a = ADXL362_ReadReg(AXDL_XDATA_ADDR); // Read raw accelerometer data x
    //     y_a = ADXL362_ReadReg(AXDL_YDATA_ADDR); // Read raw accelerometer data y
        
    //     // set the threshold=0.1 to stablize the model
    //     // the range [-90, 90] is divided into 500 steps
    //     // Limit the angle of servomotor to prevent them from reaching 90 degree
    //     if (ADXL362_Convert(x_a) > 0.1){
    //         if (*x_axis <= 550) {
    //             *x_axis = 550;
    //         }else {
    //             *x_axis -= 2;   // step down
    //         }
            
    //     }else if (ADXL362_Convert(x_a) < -0.1 ) {
    //         if (*x_axis >= 950) {
    //             *x_axis = 950;
    //         }else {
    //         *x_axis += 2;   // step up
    //         }
    //     }

    //     if (ADXL362_Convert(y_a) > 0.1){            // if the ADXL input for the y axis acceleration is larger than +0.1
    //         if (*y_axis >= 950) {
    //             *y_axis = 950;
    //         }else {
    //             *y_axis += 2;   // step up
    //         }
    //     }else if (ADXL362_Convert(y_a) < -0.1 ) {   // Otherwise if it is less than -0.1
    //         if (*y_axis <= 550) {
    //             *y_axis = 550;
    //         }else {
    //             *y_axis -= 2;   // step down
    //         }
    //     }

    //     msleep(20);
    // }
    /****************************/
    /*     END OF AUTOPILOT     */
    /****************************/



    /****************************/
    /*     JOYSTICK SECTION     */
    /****************************/
    float rx[2];
    printf("Pilot take-over.\n");
    SPI_JSTK_Init(); // initialize the joystick SPI controller
    printf("Pilot mode init done. \n");

    /* For this section, we want to record the extremities of each axis */
    printf("Follow the instructions on your screen. Push the joystick to calibrate please. \n");
    msleep(3000);
    printf("Hold to right most \n");
    msleep(3000);
    JSTK_readval(rx);
    float x_pos = rx[0];
    printf("Hold to left most \n");
    msleep(3000);
    JSTK_readval(rx);
    float x_neg = rx[0];
    printf("Hold to front most \n");
    msleep(3000);
    JSTK_readval(rx);
    float y_pos = rx[1];
    printf("Hold to rear most \n");
    msleep(3000);
    JSTK_readval(rx);
    float y_neg = rx[1];
    printf("%f", rx[0]);

    // Position calculation to convert joystick input to servo motor duty cycle on each axis
    while (1){
        JSTK_readval(rx); // read value from the joystick
        *x_axis = 550 + (x_pos - rx[0]) * (950 - 550) / (x_pos - x_neg); // limit the position step value between 500 -- 1000
        *y_axis = 550 + (rx[1] - y_neg) * (950 - 550) / (y_pos - y_neg); // limit the position step value between 500 -- 1000
        msleep(20);
    }
    /****************************/
    /*     END OF JOYSTICK      */
    /****************************/


    /****************************/
    /*     CLEANUP PLATFORM     */
    /****************************/
    cleanup_platform();
    return 0;
}