/******************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "xparameters.h"
#include "xil_cache.h"
#include "sleep.h"
#include <stdio.h>
#include <sys/_intsup.h>
#include <xil_printf.h>


#ifndef SDT
#include "platform_config.h"
#endif

#include "platform.h"

/*
 * Uncomment one of the following two lines, depending on the target,
 * if ps7/psu init source files are added in the source directory for
 * compiling example outside of SDK.
 */
/*#include "ps7_init.h"*/
/*#include "psu_init.h"*/

#ifdef STDOUT_IS_16550
#include "xuartns550_l.h"

#define UART_BAUD 9600
#endif

void enable_caches()
{
#ifdef __PPC__
	Xil_ICacheEnableRegion(CACHEABLE_REGION_MASK);
	Xil_DCacheEnableRegion(CACHEABLE_REGION_MASK);
#elif __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_ICACHE
	Xil_ICacheEnable();
#endif
#ifdef XPAR_MICROBLAZE_USE_DCACHE
	Xil_DCacheEnable();
#endif
#endif
}

void disable_caches()
{
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_DCACHE
	Xil_DCacheDisable();
#endif
#ifdef XPAR_MICROBLAZE_USE_ICACHE
	Xil_ICacheDisable();
#endif
#endif
}

void init_uart()
{
#ifdef STDOUT_IS_16550
	XUartNs550_SetBaud(STDOUT_BASEADDR, XPAR_XUARTNS550_CLOCK_HZ, UART_BAUD);
	XUartNs550_SetLineControlReg(STDOUT_BASEADDR, XUN_LCR_8_DATA_BITS);
#endif
	/* Bootrom/BSP configures PS7/PSU UART to 115200 bps */
}

void init_platform()
{
	/*
	 * If you want to run this example outside of SDK,
	 * uncomment one of the following two lines and also #include "ps7_init.h"
	 * or #include "ps7_init.h" at the top, depending on the target.
	 * Make sure that the ps7/psu_init.c and ps7/psu_init.h files are included
	 * along with this example source files for compilation.
	 */
	/* ps7_init();*/
	/* psu_init();*/
	enable_caches();
	init_uart();
}

void cleanup_platform()
{
	disable_caches();
}

/********************************************************
 * Specific Driver for IMU application
 * Author : Arnaud Dion
 * 2017
 ********************************************************/

/******************************************************************************
 * @brief Writes to an ADXL362 Register.
 *
 * @param addr - register address.
 * @param data - data to be written
 *
 * @return None.
 ******************************************************************************/
void ADXL362_WriteReg(char addr, char data)
{
	// Pointer to HW registers
	volatile unsigned long *Config_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_CONFIG_REG);		// register addr = 0x00
	volatile unsigned long *Intr_Status_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_STS_REG); // register addr = 0x04
	volatile unsigned long *Intrpt_en_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_EN_REG);	// register addr = 0x08
	volatile unsigned long *Intrpt_dis_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_DIS_REG);	// register addr = 0x0C
	volatile unsigned long *Tx_data_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_TXDATA_REG);		// register addr = 0x1C
	volatile unsigned long *Rx_data_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_RXDATA_REG);		// register addr = 0x20
	volatile unsigned long *En_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_EN_REG);				// register addr = 0x14

	// Received data from SPI
	char rx = 0x00; 

	// Procedure : See user guide ug585 v1.11 page 544
	// See 17.3.3 Master Mode Data Transfer
	// Example: Master Mode  Manual SS and Auto Start
	// Refer to ADXL362 datasheet revD
	// page 20-21

	// Point 1 :
	// It has already been done during SPI_Init()
	// Can be done here, harmless but mainly useless, so skip it
    
	// Point 2 :
	// Write configuration data to manually assert CS
    *Config_reg0 &= ~(1 << SPI_CS_Bits);
    
	// Point 3 :
	// Enable the controller
    *En_reg0 = 0x01;

	// Point 4 :
	// Write data to TX FIFO
	// Refer to ADXL362 datasheet revD
	// page 20-21
    *Tx_data_reg0 = 0x0A;   // command byte for write
	*Tx_data_reg0 = addr;   // address byte
	*Tx_data_reg0 = data;   // data byte we want to write

	// Point 5 :
	// Enable TX FIFO Not Full Interrupts
	// Useful to do the polling on TX FIFO Empty event
	// We use only the TxFIFO empty event, not RxFIFO full, RxFIFO overflow, or fault conditions.
	// So we can write 0x04 instead of 0x27
	// But it's possible to write 0x27, it doesn't change our application
    *Intrpt_en_reg0 = 1 << SPI_TX_FIFO_not_full;

	// Point 5 bis :
	// This line erase eventually remainding flag occured on a previous Tx Fifo empty event
	// See user guide ug585 v1.11
	// Annexe B.30, page 1755 and 1756
	// This line can be avoided, but it's a good practice to flush a register before polling
    *Intr_Status_reg0 = 1 << SPI_TX_FIFO_not_full;

	// Point 6 :
	// Wait for TX FIFO to empty
	// Do the polling on bit 2 of register 4
    while ((*Intr_Status_reg0 & (1 << SPI_TX_FIFO_not_full)) == 0)
		;
    
	// Clear Interrupt
	// See above Point 5 bis :
    *Intr_Status_reg0 = 1 << SPI_TX_FIFO_not_full;

	// Point 7 :
	// Not concerned because there is no more data to send
    
	// Point 8 :
	// Disable the interrupts
	// We use only the TxFIFO empty event, not RxFIFO full, RxFIFO overflow, or fault conditions.
	// So we can write 0x04 instead of 0x27
	// But it's possible to write 0x27, it doesn't change our application
    *Intrpt_dis_reg0 = 1 << SPI_TX_FIFO_not_full;

	// Point 9 :
	// Deassert /CS
	// Here there is an error in the documentation (ug585)
	// Easy to understand : The /CS should be deasserted before the controller is disabled !!
    *Config_reg0 |= (1 << SPI_CS_Bits);

	// Point 10 :
	// Disable SPI
    *En_reg0 = 0x00;

	// And Finally flush the data from RX FIFO !!
	// The data are dumped because of the simultaneous read and write on SPI bus
    while (((*Intr_Status_reg0 >> SPI_RX_FIFO_not_empty) & 0x1) != 0){
        rx = *Rx_data_reg0;
    }
}

/******************************************************************************
 * @brief Read a data from an ADXL362 Register.
 *
 * @param addr - register address.
 *
 * @return data received.
 ******************************************************************************/
char ADXL362_ReadReg(char addr)
{
	// Pointer to HW registers
	// refer to user guide ug585 v1.11
	// Appendix B.30, page 1753
	volatile unsigned long *Config_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_CONFIG_REG);		// register addr = 0x00
	volatile unsigned long *Intr_Status_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_STS_REG); // register addr = 0x04
	volatile unsigned long *Intrpt_en_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_EN_REG);	// register addr = 0x08
	volatile unsigned long *Intrpt_dis_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_DIS_REG);	// register addr = 0x0C
	volatile unsigned long *Tx_data_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_TXDATA_REG);		// register addr = 0x1C
	volatile unsigned long *Rx_data_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_RXDATA_REG);		// register addr = 0x20
	volatile unsigned long *En_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_EN_REG);				// register addr = 0x14

	// Received data from SPI
	char rx = 0x00;

	// Procedure : See user guide ug585 v1.11 page 544
	// See 17.3.3 Master Mode Data Transfer
	// Example: Master Mode � Manual SS and Auto Start
	// Refer to ADXL362 datasheet revD
	// page 20-21

	// Point 1 :
	// It has already been done during SPI_Init()
	// Can be done here, harmless but mainly useless, so skip it

	// Point 2 :
	// Write configuration data to manually assert CS
	*Config_reg0 &= ~(1 << SPI_CS_Bits);    // to clear a specific bit  

	// Point 3 :
	// Enable the controller
	*En_reg0 = 0x01;

	// Point 4 :
	// Write data to TX FIFO
	// Refer to ADXL362 datasheet revD
	// page 20-21
	*Tx_data_reg0 = 0x0B;   // the command byte for read
	*Tx_data_reg0 = addr;
	*Tx_data_reg0 = 0x00;

	// Point 5 :
	// Enable TX FIFO Not Full Interrupts
	// Useful to do the polling on TX FIFO Empty event
	// We use only the TxFIFO empty event, not RxFIFO full, RxFIFO overflow, or fault conditions.
	// So we can write 0x04 instead of 0x27
	// But it's possible to write 0x27, it doesn't change our application
	*Intrpt_en_reg0 = 1 << SPI_TX_FIFO_not_full;

	// Point 5 bis :
	// This line erase eventually remainding flag occured on a previous Tx Fifo empty event
	// See user guide ug585 v1.11
	// Annexe B.30, page 1755 and 1756
	// This line can be avoided, but it's a good practice to flush a register before polling
	*Intr_Status_reg0 = 1 << SPI_TX_FIFO_not_full;

	// Point 6 :
	// Wait for TX FIFO to empty
	// Do the polling on bit 2 of register 4
	while ((*Intr_Status_reg0 & (1 << SPI_TX_FIFO_not_full)) == 0){}
		;

	// Clear Interrupt
	// See above Point 5 bis :
	*Intr_Status_reg0 = 1 << SPI_TX_FIFO_not_full;

	// Point 7 :
	// Not concerned because there is no more data to send

	// Point 8 :
	// Disable the interrupts
	// We use only the TxFIFO empty event, not RxFIFO full, RxFIFO overflow, or fault conditions.
	// So we can write 0x04 instead of 0x27
	// But it's possible to write 0x27, it doesn't change our application
	*Intrpt_dis_reg0 = 1 << SPI_TX_FIFO_not_full;

	// Point 9 :
	// Deassert /CS
	// Here there is an error in the documentation (ug585)
	// Easy to understand : The /CS should be deasserted before the controller is disabled !!
	*Config_reg0 |= (1 << SPI_CS_Bits);

	// Point 10 :
	// Disable SPI
	*En_reg0 = 0x00;

	// And Finally read the data from RX FIFO !!
	// The first 2 data are dumped because of the simultaneous read and write on SPI bus
	rx = *Rx_data_reg0;
	rx = *Rx_data_reg0;
	// Useful Data
	rx = *Rx_data_reg0;
	// Send useful data
	return (rx);
}

/******************************************************************************
 * @brief Initialization of the accelerometer for the measurements.
 *
 * @param None.
 *
 * @return None.
 ******************************************************************************/
void ADXL362_Init(void)
{
	// Software Reset
	// Refer to ADXL362datasheet revD
	// page 27
	ADXL362_WriteReg(ADXL362_SOFT_RESET, ADXL362_RESET_CMD);
	msleep(10);
	ADXL362_WriteReg(ADXL362_SOFT_RESET, 0x00);

	// Enable Measurement
	// Refer to ADXL362datasheet revD
	// page 35
	ADXL362_WriteReg(ADXL362_POWER_CTL, (2 << ADXL362_MEASURE));
	msleep(500);
}

/******************************************************************************
 * @brief Set the range of the acceleration to measure.
 *
 * @param None.
 *
 * @return None.
 ******************************************************************************/
void ADXL362_SetRange(void)
{
	// Refer to datasheet revD
	// page 34

	// Local copy (stored in memory) of HW register
	char _FILTER_CTL = 0;

	_FILTER_CTL = (ADXL362_ReadReg(ADXL362_FILTER_CTL) & 0x3F);

	// Set to +/-2g range
	_FILTER_CTL = 0x00 | _FILTER_CTL;

	ADXL362_WriteReg(ADXL362_FILTER_CTL, _FILTER_CTL);
}

/******************************************************************************
 * @brief Initialization of the SPI controler.
 *
 * @param None.
 *
 * @return None.
 ******************************************************************************/
void SPI_ADXL_Init(void)
{
	// Pointer to HW registers
	volatile unsigned long *Config_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_CONFIG_REG);
	volatile unsigned long *Intr_Status_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_STS_REG);
	volatile unsigned long *Rx_data_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_RXDATA_REG);
	volatile unsigned long *TX_thres_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_TX_THRES_REG);
	volatile unsigned long *En_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_EN_REG); // register addr = 0x14
	// Received data from SPI
	unsigned long DumpData = 0;

	*Config_reg0 = 0x00020000;
	// Configuration Register Settings
	// Refer to user guide ug585 v1.11
	// Appendix B.30, page 1754
	*Config_reg0 |= (1 << SPI_Modefail_gen_en) |
					(0 << SPI_Man_start_com) |
					(0 << SPI_Man_start_en) |
					(1 << SPI_Manual_CS) |
					(0x0F << SPI_CS_Bits) |
					(0 << SPI_PERI_SEL) |
					(0 << SPI_REF_CLK) |
					(4 << SPI_BAUD_RATE_DIV) |
					(0 << SPI_CLK_PH) |
					(0 << SPI_CLK_POL) |
					(1 << SPI_MODE_SEL);

	// Write TX FIFO Not Full Level
	*TX_thres_reg0 = 0x01;

	// Flush FIFO
	while ((*Intr_Status_reg0 & BIT(SPI_RX_FIFO_not_empty)) != 0)
	{
		DumpData = *Rx_data_reg0;
	}
}

/******************************************************************************
 * @brief Poll the ADXL362 for data.
 *
 * @param None.
 *
 * @return char : 1 it data, 0 otherwise.
 ******************************************************************************/
char ADXL362_IsDataReady(void)
{
	// Point 6 :
	// Wait for TX FIFO to empty
	// Do the polling on bit 2 of register 4
	return (ADXL362_ReadReg(ADXL362_STATUS) & (1 << ADXL362_DATA_READY)) == 0x01;
}

/******************************************************************************
 * @brief Return an acceleration value in floating point.
 *
 * @param rxG - Raw acceleration value.
 *
 * @return floating point acceleration.
 ******************************************************************************/
float ADXL362_Convert(char rx)
{
	int sign = 0;
	float frxG = 0.0;
	int rxG = 0;

	// Sign is MSB. If 1 -> Negative Number, else Positive Number
	sign = (rx & 0x80) >> 7;

	// Shift by 4 bits
	// Because we read only the MSB data of a 12-bit data
	// Refer to ADXL362datasheet revD
	// page 25
	rxG = (rx << 4);
	if (sign == 1)
		rxG |= 0xFFFFF000; // If negative number, padding with FFFFF MSB's

	// Sensitivity for +/-2g range
	//  Refer to ADXL362datasheet revD
	//  table 1, page 5
	frxG = ((float)rxG / 1000.0);

	return frxG;
}

/******************************************************************************
 * @brief Print ID of ADXL362.
 *
 * @param None
 *
 * @return none.
 ******************************************************************************/
void ADXL362_PrintID(void)
{
	printf("\n\r");
	printf("Device ID:  0x%X\n\r", ADXL362_ReadReg(ADXL362_DEVID_AD) & 0xFF);
	printf("Device ID:  0x%X\n\r", ADXL362_ReadReg(ADXL362_DEVID_MST) & 0xFF);
	printf("Part ID:    0x%X\n\r", ADXL362_ReadReg(ADXL362_PARTID) & 0xFF);
	printf("Silicon ID: 0x%X\n\r", ADXL362_ReadReg(ADXL362_REVID) & 0xFF);
	printf("\n\r");
}

/******************************************************************************
 * @brief Initialization of the SPI controler for Joystick.
 *
 * @param None.
 *
 * @return None.
 ******************************************************************************/
void SPI_JSTK_Init(void)
{
	// Pointer to HW registers
	volatile unsigned long *Config_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_CONFIG_REG);
	volatile unsigned long *Intr_Status_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_STS_REG);
	volatile unsigned long *Rx_data_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_RXDATA_REG);
	volatile unsigned long *TX_thres_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_TX_THRES_REG);
	volatile unsigned long *En_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_EN_REG); // register addr = 0x14
	// Received data from SPI
	unsigned long DumpData = 0;

	*Config_reg0 = 0x00020000;

	// Be careful, the base clock for baudrate is 128MHz
	// *Config_reg0 |= BIT(SPI_Modefail_gen_en) |
	// 				BIT(SPI_Manual_CS) |
	// 				BIT(SPI_Man_start_en) |
	// 				BIT(SPI_CS_Bits) * 0xF |
	// 				BIT(SPI_BAUD_RATE_DIV) * 6 |
	// 				BIT(SPI_MODE_SEL);
    // // Config_reg0 = 0x2fc31 => bit 13:10: 1111 No slave selected 
    // Here the BAUD_RATE_DIV field is 110 (110: divide by 128) page 1720 - NOT WORKING !!!!

    /* Modified configuration registers from the provided */
    *Config_reg0 |= (1 << SPI_Modefail_gen_en) |
					(0 << SPI_Man_start_com) |
					(0 << SPI_Man_start_en) |
					(1 << SPI_Manual_CS) |
					(0x0F << SPI_CS_Bits) |
					(0 << SPI_PERI_SEL) |
					(0 << SPI_REF_CLK) |
					(4 << SPI_BAUD_RATE_DIV) |
					(0 << SPI_CLK_PH) |
					(0 << SPI_CLK_POL) |
					(1 << SPI_MODE_SEL);
    // Config_reg0 = 0x2fc21 => bit 13:10: 1111 No slave selected
    // Here the BAUD_RATE_DIV field is 010 (010: divide by 8) page 1720 - IT WORKS !!!!!!!!!
    // REMARK: Changing the baud rate divisor to the same used in acclerometer fixed it.
    //         However, we are not sure about the reason. 


    // printf("Config_reg = 0x%x \n", *Config_reg0);   
	// Write TX FIFO Not Full Level
	*TX_thres_reg0 = 1;
	// Flush RX FIFO
	while ((*Intr_Status_reg0 & BIT(SPI_RX_FIFO_not_empty)) != 0)
	{
		DumpData = *Rx_data_reg0;
	}
    printf("Joystick SPI initialized. \n");
}

/******************************************************************************
 * @brief Read the value of axis and button from Joystick.
 *
 * @param Pointer to an array.
 *
 * @return None.
 ******************************************************************************/

void JSTK_readval(float *rx)
{   
	volatile unsigned long *Config_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_CONFIG_REG);        // page 1719 Relative Address 0x00000000
	volatile unsigned long *Intr_Status_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_STS_REG); // page 1721 Relative Address 0x00000004
	volatile unsigned long *Tx_data_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_TXDATA_REG);       // page 1727 Relative Address 0x0000001C 
	volatile unsigned long *Rx_data_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_RXDATA_REG);       // page 1727 Relative Address 0x00000020 
	volatile unsigned long *En_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_EN_REG);                // page 1725 Relative Address 0x00000014
    volatile unsigned long *Intrpt_en_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_EN_REG);	// page 1722 Relative Address 0x00000008
	volatile unsigned long *Intrpt_dis_reg0 = (unsigned long *)(XPAR_SPI1_BASEADDR + HW_SPI_INTR_DIS_REG);	// page 1723 Relative Address 0x0000000C

	uint8_t rxdata[5];
	// Enable the controller
	*En_reg0 = 0x01;

	// assert CS
	BITCLR(*Config_reg0, JSTK_CS);  // JSTK_CS = 0b100000000000  bit 13:10: xx01 - slave 1 selected
    // Config_reg0 = 0b1111010000110001 (0x2f431) => master mode | Manual CS | Manual Start enable
    // printf("Config_reg0 = 0x%x \n", *Config_reg0); //DEBUG
	usleep(15); // >=15µs wait after putting controller to high

    *Intrpt_en_reg0 |= 1 << SPI_TX_FIFO_not_full; // enable interrupts
    // printf("TX FIFO Not Full Interrupts was enabled. \n");

	// Write data to TX FIFO
    *Tx_data_reg0 = JSTK_GET_POS_CMD;   // command byte
    usleep(10); // wait 10µs before sending the next byte - refer to pmod jstk manual page 5
    *Tx_data_reg0 = JSTK_DUMMY;         // dummy byte 1
    usleep(10);
    *Tx_data_reg0 = JSTK_DUMMY;         // dummy byte 2
    usleep(10); 
    *Tx_data_reg0 = JSTK_DUMMY;         // dummy byte 3
    usleep(10); 
    *Tx_data_reg0 = JSTK_DUMMY;         // dummy byte 4
    usleep(10);
    // printf("Dummy bytes were sent. \n"); //DEBUG

    // Flush previous TX empty flag
    *Intr_Status_reg0 |= 1 << SPI_TX_FIFO_not_full;
    // printf("TX Status was flushed. \n"); //DEBUG

	// Wait for TX FIFO to empty
    while ((*Intr_Status_reg0 & (1 << SPI_TX_FIFO_not_full)) == 0){
        // printf("Intr_Status_reg0 = 0x%08lx\n", *Intr_Status_reg0); //DEBUG
    }
    // printf("Interrupt by empty TX \n"); //DEBUG

    // Clear the interrupts
    *Intr_Status_reg0 = 1 << SPI_TX_FIFO_not_full;
    // printf("Interrupt cleared. \n"); //DEBUG

    // Disable the interrupts
    *Intrpt_dis_reg0 = 1 << SPI_TX_FIFO_not_full;   
    // printf("Interruption disabled. \n"); //DEBUG

	// Easy to understand : The /CS should be deasserted before the controller is disabled !!
	BITSET(*Config_reg0, JSTK_CS);
    // printf("Deassert controller. \n"); //DEBUG

	// Disable SPI
	*En_reg0 = 0x00;
    // xil_printf("SPI disabled. \n"); //DEBUG

    // Reading from RX
    *rxdata = *Rx_data_reg0;
	*(rxdata+0x01) = *Rx_data_reg0;
    *(rxdata+0x02) = *Rx_data_reg0;
	*(rxdata+0x03) = *Rx_data_reg0;
	*(rxdata+0x04) = *Rx_data_reg0;
    // xil_printf("Finished reading from RX. \n"); //DEBUG

    // Concatenate for meaningful data 
    // refer to pmod_jstk manual page 2
    uint16_t uint16_x = (rxdata[1] << 8) | rxdata[0];   
    rx[0] = (float) uint16_x;
    uint16_t uint16_y = (rxdata[3] << 8) | rxdata[2];
    rx[1] = (float) uint16_y;
    // xil_printf("Reading routing finished. \n"); //DEBUG
}
