/*****************************************************************************/
/**
 *
 * @file xmulti_scaler_example.c
 *
 * This is main file for the MultiScaler example design.
 *
 * The MultiScaler HW configuration is detected and several use cases are
 * selected to test it.
 *
 * On start-up the program reads the HW config and initializes its internal
 * data structures. The interrupt handler is registered.
 * The set of use cases mentioned in the array are selected.
 * Testing a use case is done by:
 *	1) Read the destination buffer before scaling.
 *	2) Program the parameters mentioned in the use case to the HW registers.
 *	3) Start the HW, and when the scaling completes interrupt handler is
 *	invoked. In the interrupt handler again read the contents of the
 *	destination buffer to verify that the scaled data is available in the
 *	destination buffer.
 *	4) Go back and set up the next use case, repeating steps 1,2,3.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xil_printf.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "xv_multi_scaler_l2.h"
#include "xuartlite_l.h"

//============Zynq============
//#include "xscugic.h"
//============================

//==========MicroBlaze========
#include "xintc.h"
//============================

/************************** Local Constants *********************************/
#define XMULTISCALER_SW_VER "v1.00"
#define XGPIOPS_MASK_DATA_3_LSW_OFFSET	0x00000018
#define XGPIOPS_DIRM_3_OFFSET	0x00000004
#define XGPIOPS_OEN_3_OFFSET	0x00000008
#define XGPIOPS_DATA_3_OFFSET	0x00000000
#define USECASE_COUNT 1

//eddy modify
//#define XNUM_OUTPUTS 2
#define XNUM_OUTPUTS 1

#define SRC_BUF_START_ADDR 0x90000000
#define DST_BUF_START_ADDR 0xA0000000

//---change to microblaze lib
XIntc Intc;
//XScuGic Intc;

XV_multi_scaler MultiScalerInst;
XV_multi_scaler_Video_Config *thisCase;
u32 interrupt_flag;

// list of use cases
/*
 typedef enum
 {
 XV_MULTI_SCALER_RGBX8  = 10,
 XV_MULTI_SCALER_YUVX8  = 11,
 XV_MULTI_SCALER_YUYV8 = 12,
 XV_MULTI_SCALER_RGBX10 = 15,
 XV_MULTI_SCALER_YUVX10 = 16,
 XV_MULTI_SCALER_Y_UV8 = 18,
 XV_MULTI_SCALER_Y_UV8_420 = 19,
 XV_MULTI_SCALER_RGB8 = 20,
 XV_MULTI_SCALER_YUV8 = 21,
 XV_MULTI_SCALER_Y_UV10 = 22,
 XV_MULTI_SCALER_Y_UV10_420 = 23,
 XV_MULTI_SCALER_Y8 = 24,
 XV_MULTI_SCALER_Y10 = 25,
 XV_MULTI_SCALER_BGRX8 = 27,
 XV_MULTI_SCALER_UYVY8 = 28,
 XV_MULTI_SCALER_BGR8 = 29,
 } XV_MULTISCALER_MEMORY_FORMATS;
 */

/*
 typedef struct {
 u32 Height;
 u32 Width;
 u32 StartX;
 u32 StartY;
 u8 Crop;
 } XV_multi_scaler_Crop_Window;
 */

XV_multi_scaler_Video_Config useCase[USECASE_COUNT][XNUM_OUTPUTS] = { { {
		0,//u32 ChannelId
		SRC_BUF_START_ADDR,//UINTPTR SrcImgBuf0
		SRC_BUF_START_ADDR + 8 *XV_MAX_BUF_SIZE,//UINTPTR SrcImgBuf1

		720,//u32 HeightIn
		825,//u32 HeightOut
		720,//u32 WidthIn
		1765,//u32 WidthOut

		/*
		64,//u32 HeightIn
		128,//u32 HeightOut
		64,//u32 WidthIn
		128,//u32 WidthOut
		*/
		XV_MULTI_SCALER_RGB8,//u32 ColorFormatIn
		XV_MULTI_SCALER_RGB8,//u32 ColorFormatOut
		0,//u32 InStride
		0,//u32 OutStride
		DST_BUF_START_ADDR,//UINTPTR DstImgBuf0
		DST_BUF_START_ADDR + 7 *XV_MAX_BUF_SIZE,//UINTPTR DstImgBuf1
		{0, 0, 0, 0, 0}//XV_multi_scaler_Crop_Window CropWin
	} } };

/*****************************************************************************/
/**
 *
 * This function setups the interrupt system so interrupts can occur for the
 * multiscaler core.
 *
 * @return
 *	- XST_SUCCESS if interrupt setup was successful.
 *	- A specific error code defined in "xstatus.h" if an error
 *	occurs.
 *
 * @note	This function assumes a Microblaze or ARM system and no
 *	operating system is used.
 *
 ******************************************************************************/
static int SetupInterruptSystem(void) {
	int Status;

	//---change to microblaze lib
	//XScuGic *IntcInstPtr = &Intc;
	XIntc *IntcInstPtr = &Intc;

	/*
	 * Initialize the interrupt controller driver so that it's ready to
	 * use, specify the device ID that was generated in xparameters.h
	 */
	//==============================================Zynq=================================================
	////XScuGic_Config *IntcCfgPtr;
	////
	//////IntcCfgPtr = XScuGic_LookupConfig(XPAR_PSU_ACPU_GIC_DEVICE_ID);//eddy remove
	////IntcCfgPtr = XScuGic_LookupConfig(XPAR_AXI_INTC_0_DEVICE_ID);//eddy add
	////
	////if (!IntcCfgPtr) {
	////	xil_printf("ERR:: Interrupt Controller not found");
	////	return XST_DEVICE_NOT_FOUND;
	////}
	//===================================================================================================

	//==============================================MicroBlaze=================================================
	//Status = XScuGic_CfgInitialize(IntcInstPtr, IntcCfgPtr, IntcCfgPtr->CpuBaseAddress);
	Status = XIntc_Initialize(IntcInstPtr, XPAR_INTC_0_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("STEP1 : XIntc initialization failed!\r\n");
		return XST_FAILURE;
	} else {
		xil_printf("STEP1 : XIntc initialization passed!\r\n");
	}

	//Starts the interrupt controller by enabling the output from the controller to the processor
	Status = XIntc_Start(IntcInstPtr, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {
		xil_printf("STEP2 : Starts the interrupt controller failed!\r\n");
		return XST_FAILURE;
	} else {
		xil_printf("STEP2 : Starts the interrupt controller passed!\r\n");
	}
	//===================================================================================================

	Xil_ExceptionInit();//The function is a common API used to initialize exception handlers across all supported arm processors

	/*
	 * Register the interrupt controller handler with the exception table.
	 */
	//==============================================Zynq=================================================
	//Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
	//	(Xil_ExceptionHandler) XScuGic_InterruptHandler,
	//	(XScuGic *)IntcInstPtr);
	//===================================================================================================
	//==============================================MicroBlaze=================================================
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler) XIntc_InterruptHandler,
			(XIntc *) IntcInstPtr);
	xil_printf(
			"STEP3 : Register the interrupt controller handler with the exception table done \r\n");
	//===================================================================================================

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
 *
 * This function drives the reset pin of core high to start multiscaler core.
 *
 * @return
 *
 * @note	This function assumes a Microblaze or ARM system and no
 *	operating system is used.
 *
 ******************************************************************************/
void XV_Reset_MultiScaler(void) {
	//eddy remove
	/*
	 XV_multi_scaler_WriteReg(XPAR_PSU_GPIO_0_BASEADDR, XGPIOPS_MASK_DATA_3_LSW_OFFSET, 0xFFFF0000);
	 XV_multi_scaler_WriteReg(XPAR_PSU_GPIO_0_BASEADDR, XGPIOPS_DIRM_3_OFFSET, 0xFFFFFFFF);
	 XV_multi_scaler_WriteReg(XPAR_PSU_GPIO_0_BASEADDR, XGPIOPS_OEN_3_OFFSET , 0xFFFFFFFF);
	 XV_multi_scaler_WriteReg(XPAR_PSU_GPIO_0_BASEADDR, XGPIOPS_DATA_3_OFFSET, 0x00000001);
	 XV_multi_scaler_WriteReg(XPAR_PSU_GPIO_0_BASEADDR, XGPIOPS_DATA_3_OFFSET, 0x00000000);
	 XV_multi_scaler_WriteReg(XPAR_PSU_GPIO_0_BASEADDR, XGPIOPS_DATA_3_OFFSET, 0x00000001);
	 */

	//eddy add
	XV_multi_scaler_WriteReg(XPAR_AXI_GPIO_0_BASEADDR,
			XGPIOPS_MASK_DATA_3_LSW_OFFSET, 0xFFFF0000);
	XV_multi_scaler_WriteReg(XPAR_AXI_GPIO_0_BASEADDR, XGPIOPS_DIRM_3_OFFSET,
			0x00000000);
	XV_multi_scaler_WriteReg(XPAR_AXI_GPIO_0_BASEADDR, XGPIOPS_OEN_3_OFFSET,
			0xFFFFFFFF);
	XV_multi_scaler_WriteReg(XPAR_AXI_GPIO_0_BASEADDR, XGPIOPS_DATA_3_OFFSET,
			0x00000001);
	XV_multi_scaler_WriteReg(XPAR_AXI_GPIO_0_BASEADDR, XGPIOPS_DATA_3_OFFSET,
			0x00000000);
	XV_multi_scaler_WriteReg(XPAR_AXI_GPIO_0_BASEADDR, XGPIOPS_DATA_3_OFFSET,
			0x00000001);
}

/*****************************************************************************/
/**
 *
 * This function is called from the interrupt handler of multiscaler core.
 * After the first interrupt is received the interrupt_flag is set here and
 * it stops the multi scaler core.
 *
 * @return
 *
 * @note	This function assumes a Microblaze or ARM system and no
 *	operating system is used.
 *
 ******************************************************************************/
void *XVMultiScalerCallback(void *data) {
	xil_printf("\nMultiScaler interrupt received.\r\n");

	/* clear interrupt flag */
	interrupt_flag = 0;

	XV_multi_scaler_InterruptGlobalDisable(&MultiScalerInst);
	XV_multi_scaler_InterruptDisable(&MultiScalerInst, 0xF);
	XV_MultiScalerStop(&MultiScalerInst);
}

/*****************************************************************************/
/**
 * This function calculates the stride
 *
 * @returns stride in bytes
 *
 *****************************************************************************/
static u32 CalcStride(u16 Cfmt, u16 AXIMMDataWidth, u32 width) {
	u32 stride;
	u16 MMWidthBytes = AXIMMDataWidth / 8;
	u8 bpp_numerator;
	u8 bpp_denominator = 1;

	switch (Cfmt) {
	case XV_MULTI_SCALER_Y_UV10:
	case XV_MULTI_SCALER_Y_UV10_420:
	case XV_MULTI_SCALER_Y10:
		/* 4 bytes per 3 pixels (Y_UV10, Y_UV10_420, Y10) */
		bpp_numerator = 4;
		bpp_denominator = 3;
		break;
	case XV_MULTI_SCALER_Y_UV8:
	case XV_MULTI_SCALER_Y_UV8_420:
	case XV_MULTI_SCALER_Y8:
		/* 1 byte per pixel (Y_UV8, Y_UV8_420, Y8) */
		bpp_numerator = 1;
		break;
	case XV_MULTI_SCALER_RGB8:
	case XV_MULTI_SCALER_YUV8:
	case XV_MULTI_SCALER_BGR8:
		/* 3 bytes per pixel (RGB8, YUV8, BGR8) */
		bpp_numerator = 3;
		break;
	case XV_MULTI_SCALER_YUYV8:
	case XV_MULTI_SCALER_UYVY8:
		/* 2 bytes per pixel (YUYV8, UYVY8) */
		bpp_numerator = 2;
		break;
	default:
		/* 4 bytes per pixel */
		bpp_numerator = 4;
	}
	stride = ((((width * bpp_numerator) / bpp_denominator) + MMWidthBytes - 1)
			/ MMWidthBytes) * MMWidthBytes;

	return stride;
}

int main(void) {
	XV_multi_scaler *MultiScalerPtr;

	u32 status;
	u32 k;
	u32 i;
	u32 j;
	u32 cnt = 0;
	u32 width_in_words = 0;
	u32 width_out_words = 0;
	_Bool flag = 1;
	u32 num_outs;

	//1. Bind instance pointer with definition
	MultiScalerPtr = &MultiScalerInst;

	//2. Instruction Cache
	Xil_ICacheInvalidate()
	;	//Invalidate the entire instruction cache
	Xil_ICacheDisable();	//Disable the instruction cache

	//3. Data Cache
	Xil_DCacheInvalidate()
	;	//Invalidate the Data cache
	Xil_DCacheDisable();	//Disable the Data cache

	//4. Disable the IRQ exception
	Xil_ExceptionDisable();

	xil_printf("\r\n-----------------------------------------------\r\n");
	xil_printf(" Xilinx Multi Scaler Example Design %s\r\n",
			XMULTISCALER_SW_VER);
	xil_printf("	(c) 2018 by Xilinx Inc.\r\n");

	//5. Initialize IRQ
	status = SetupInterruptSystem();
	if (status == XST_FAILURE) {
		xil_printf("STEP4 : IRQ init failed.\n\r");
		return XST_FAILURE;
	} else {
		xil_printf("STEP4 : IRQ init passed.\n\r");
	}

	//6. Initialize Scaler IP
	status = XV_multi_scaler_Initialize(MultiScalerPtr,
			XPAR_V_MULTI_SCALER_0_DEVICE_ID);
	if (status != XST_SUCCESS) {
		xil_printf("STEP5 : CRITICAL ERROR:: System Init Failed.\n\r");
		return XST_FAILURE;
	} else {
		xil_printf("STEP5 : System Init Passed.\n\r");
	}

	//7. Installs an asynchronous callback function
	XVMultiScaler_SetCallback(MultiScalerPtr, XVMultiScalerCallback,
			(void *) MultiScalerPtr);
	xil_printf("STEP6 : Installs an asynchronous callback function done.\n\r");

	// 8. Makes the connection between the Int_Id of the interrupt source and the
	//associated handler that is to run when the interrupt is recognized

	//---change to microblaze lib
	//status = XScuGic_Connect( //XScuGic_Connect(XScuGic *InstancePtr, u32 Int_Id, Xil_InterruptHandler Handler, void *CallBackRef)
	status = XIntc_Connect( //XIntc_Connect(XIntc * InstancePtr, u8 Id, XInterruptHandler Handler, void *CallBackRef)
			&Intc,
			//XPAR_FABRIC_V_MULTI_SCALER_0_INTERRUPT_INTR,//eddy remove
			XPAR_INTC_0_V_MULTI_SCALER_0_VEC_ID,//eddy add
			(XInterruptHandler) XV_MultiScalerIntrHandler, //This function is the interrupt handler for the MultiScaler core driver
			(void *) MultiScalerPtr);
	if (status == XST_SUCCESS)
	//{XScuGic_Enable(&Intc, XPAR_FABRIC_V_MULTI_SCALER_0_INTERRUPT_INTR);}//eddy remove
	//{XScuGic_Enable(&Intc, XPAR_AXI_INTC_0_V_MULTI_SCALER_0_INTERRUPT_INTR);}	//eddy add
	{
		XIntc_Enable(&Intc, XPAR_AXI_INTC_0_V_MULTI_SCALER_0_INTERRUPT_INTR);//---change to microblaze lib
		xil_printf("STEP7 : able to register interrupt handler.\n\r");
	} else {
		xil_printf("STEP7 : ERR:: Unable to register interrupt handler.\n\r");
		return XST_FAILURE;
	}

	//9. Enable the exception
	Xil_ExceptionEnable();
	xil_printf("STEP8 : Enable the IRQ exception done.\n\r");

	while (cnt < USECASE_COUNT) {
		xil_printf("STEP9 : Use case %d:\n\r", cnt);

		//10. Drives the reset pin of core high to start multiscaler core
		XV_Reset_MultiScaler();
		xil_printf(
				"STEP10 : Drives the reset pin of core high to start multiscaler core.\n\r");

		//11. Sets the number of outputs
		XV_MultiScalerSetNumOutputs(MultiScalerPtr, XNUM_OUTPUTS);
		xil_printf("STEP11 : Sets the number of outputs.\n\r");

		//12. Returns the number of output
		num_outs = XV_MultiScalerGetNumOutputs(MultiScalerPtr);
		xil_printf("STEP12 : Detect Mult-Scaler Num Outputs Is %d\n\r",
				num_outs);
		if (num_outs != XNUM_OUTPUTS) {
			xil_printf("\nERR:: Incorrect number of outputs\n");
			return XST_FAILURE;
		}
		if (num_outs != XNUM_OUTPUTS) {
			xil_printf("STEP13 : ERR:: Incorrect number of outputs\n\r");
			return XST_FAILURE;
		} else {
			xil_printf("STEP13 : Correct number of outputs\n\r");
		}

		//13. Fill the src buffer with a fixed pattern
		for (k = 0; k < num_outs; k++) {
			thisCase = &useCase[cnt][k];
			xil_printf(
					"STEP14 : Fill the src buffer with a fixed pattern - useCase cnt=%d , k=%d \n\r",
					cnt, k);
			xil_printf(
					"srcBuffer0=%x  srcBuffer1=%x  DstImgBuf0=%x  DstImgBuf1=%x\r\n",
					(u32 *) thisCase->SrcImgBuf0, (u32 *) thisCase->SrcImgBuf1,
					(u32 *) thisCase->DstImgBuf0, (u32 *) thisCase->DstImgBuf1);
			//calculates the stride
			//static u32 CalcStride(u16 Cfmt, u16 AXIMMDataWidth, u32 width)
			thisCase->InStride = CalcStride(thisCase->ColorFormatIn,
					MultiScalerPtr->MaxDataWidth, thisCase->WidthIn);
			thisCase->OutStride = CalcStride(thisCase->ColorFormatOut,
					MultiScalerPtr->MaxDataWidth, thisCase->WidthOut);

			memset((u32 *) thisCase->SrcImgBuf0, 0x00,
					thisCase->HeightIn * thisCase->InStride);
			memset((u32 *) thisCase->SrcImgBuf1, 0x00,
					thisCase->HeightIn * thisCase->InStride);
			memset((u32 *) thisCase->DstImgBuf0, 0xAA,
					thisCase->HeightOut * thisCase->OutStride + 10);
			memset((u32 *) thisCase->DstImgBuf1, 0xAA,
					thisCase->HeightOut * thisCase->OutStride + 10);

			//14. configures the scaler core registers
			XV_MultiScalerSetChannelConfig(MultiScalerPtr, thisCase);
			xil_printf("STEP15 : configures the scaler core registers\n\r");
		}

		//15. Enable Interrupt
		XV_multi_scaler_InterruptGlobalEnable(MultiScalerPtr);
		XV_multi_scaler_InterruptEnable(MultiScalerPtr, 0xF);

		xil_printf("STEP16 : Enable Interrupt.\r\n");

		interrupt_flag = 1;

		//16. starts the multi scaler core
		XV_MultiScalerStart(MultiScalerPtr);
		xil_printf("STEP17 : Start MultiScaler Core.\r\n");

		//17. wait for interrupt
		while (interrupt_flag == 1)
			;

		/* Check if the data in the destination buffer is proper*/
		for (k = 0; k < XNUM_OUTPUTS; k++) {
			xil_printf(
					"STEP18 : Check if the data in the destination buffer is proper\r\n");

			thisCase = &useCase[cnt][k];
			//xil_printf("Test Case : cnt = %d , k = %d \n\r", cnt , k);

			width_in_words = thisCase->InStride / 4;
			width_out_words = thisCase->OutStride / 4;
			//xil_printf("width_in_words = %d , width_out_words = %d \n\r", width_in_words , width_out_words);

			for (i = 0; i < thisCase->HeightOut; i++) {
				for (j = 0; j < width_out_words; j++) {
					if (*((u32 *) thisCase->DstImgBuf0 + width_out_words * i + j)
							!= 0x00) {
						xil_printf("\ndata mismatch");
						xil_printf("addr=%x data=%x\n",
								(u32 *) thisCase->DstImgBuf0
										+ width_out_words * i + j,
								*((u32 *) thisCase->DstImgBuf0
										+ width_out_words * i + j));
						flag = 0;
						break;
					}
				}
				if (flag == 0) {
					break;
				}
			}
			if (*((u32 *) thisCase->DstImgBuf0
					+ thisCase->HeightOut * width_out_words + 1)
					!= 0xAAAAAAAA) {
				flag = 0;
				xil_printf("Destination buffer overflown\r\n");
			} else {
				xil_printf("Destination buffer not overflown\r\n");
			}
		}
		if (flag == 0) {
			xil_printf("Test case %d failed\n", cnt);
			break;
		}

		xil_printf("\nEnd testing this use case. \r\n");
		cnt++;
	}
	if (cnt == USECASE_COUNT)
		xil_printf("\nMultiScaler test successful. \r\n");
	else
		xil_printf("MultiScaler test failed. \r\n");

	return 0;
}
