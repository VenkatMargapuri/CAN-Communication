#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>

#include <libopencm3/stm32/rcc.h>
 #include <libopencm3/stm32/gpio.h>
 #include <libopencm3/stm32/adc.h>
 #include <libopencm3/stm32/f4/nvic.h>
 #include <libopencm3/stm32/f4/exti.h>
 #include <libopencm3/cm3/systick.h>


#include "./rtos/FreeRTOS.h"
#include "../include/rtos/canmsgs.h"
#include "./rtos/task.h"
#include "./MayorSrc/ck.h"
#include "./KingSrc/kCk.h"

struct s_canmsg cmsg;
bool xmsgidf, rtrf;


/* Set STM32 to 168 MHz. */
static void clock_setup(void)
{
	rcc_clock_setup_hse_3v3(&hse_8mhz_3v3[CLOCK_3V3_168MHZ]);		
}

/* Receive a CAN Message */
struct s_canmsg receive(uint8_t fifo){

    can_receive(CAN1,
                fifo, // FIFO id
                true, // Automatically release FIFO after rx
                &cmsg.msgid, &xmsgidf, &rtrf, (uint32_t *) &cmsg.fmi,
                &cmsg.length, cmsg.data);

	cmsg.xmsgidf = xmsgidf;
	cmsg.rtrf = rtrf;
	cmsg.fifo = fifo;

	return cmsg;
	
}

/* Checks for a reaction document and toggles an LED if received with the correct information */
void ckMayor2()
{
	uint8_t result = checkReactionDocument();
	if(result == 1){
		gpio_toggle(GPIOD, GPIO13);
	}
}

/* Interrupt Service Routine for CAN1 Peripheral */
void can1_rx0_isr(void)
{	
	/* The function for the mayor to receive information from the King to set the Action document */
	//ckMain();

	/* The function for the mayor to receive a reaction document from another mayor */
	ckMayor2();	
}

static void exti_setup(void)
{
	/* Enable GPIOA clock. */
	rcc_periph_clock_enable(RCC_GPIOA);

	/* Enable EXTI0 interrupt. */
	nvic_enable_irq(NVIC_EXTI0_IRQ);

	/* Set GPIO0 (in GPIO port A) to 'input float'. */
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO0);

	/* Configure the EXTI subsystem. */
	exti_select_source(EXTI0, GPIOA);
	exti_set_trigger(EXTI0, EXTI_TRIGGER_FALLING);
	exti_enable_request(EXTI0);	
}

/* Function that Handles PA0 (button) Interrupt */
void exti0_isr(void)
{	
	ckMP1(1, 5, 3, 1, 4, 1);
	exti_reset_request(EXTI0);
}

/* Enables the CAN Receive Interrupts upon Receiving a CAN Message */
void EnableCANRxISR(){
	nvic_enable_irq(NVIC_CAN1_RX0_IRQ);
	can_enable_irq(CAN1, NVIC_CAN1_RX0_IRQ);	
	nvic_set_priority(NVIC_CAN1_RX0_IRQ, 0);
}


/*********************************************************************
 * Main program: Device initialization etc.
 *********************************************************************/
int
main(void) {
	
	//Enable clock to the CAN peripheral
	rcc_periph_clock_enable(RCC_CAN1);
	rcc_periph_clock_enable(RCC_GPIOD);
	rcc_periph_clock_enable(RCC_GPIOB);		
	
	/* Setup GPIO pins for CAN1 transmit. */
	 gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);
	
	 /* Setup GPIO pins for CAN1 receive. */
	 gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO8);
	 gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_25MHZ, GPIO8);

	 /* Setup CAN1 TX and RX pin as alternate function. */
	 gpio_set_af(GPIOB, GPIO_AF9, GPIO8);
	 gpio_set_af(GPIOB, GPIO_AF9, GPIO9);
	
	 /* Initializing the pins for the LEDs */
	 gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO12);
	 gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
	 gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO14);
	 gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);

	 /* Reset the CAN Peripheral */
	 can_reset(CAN1);								 
	 	
	//Transmission speed of 1 Mbps
	can_init(CAN1,
	false,
	true,
	true,
	false,
	false,
	false,
	CAN_BTR_SJW_1TQ,
	CAN_BTR_TS1_6TQ,
	CAN_BTR_TS2_7TQ,
	3,
	false,
	false	
	);	
	
	/* Enable this filter and disable all other filters to accept all messages */
	can_filter_id_mask_32bit_init(CAN1,
				0,     /* Filter ID */
				0,     /* CAN ID */
				0,     /* CAN ID mask */
				0,     /* FIFO assignment (here: FIFO0) */
				true); /* Enable the filter. */		

	can_enable_irq(CAN1, CAN_IER_FMPIE0);
	can_enable_irq(CAN1, CAN_IER_FMPIE1);		

	/* Function that sets the clock on the STM32F4 Discovery Board */
	clock_setup();

	/* Function that sets the external interrupt (button) for PA0 on the STM32F4 Discovery Board */
	exti_setup();

	/* Enables the CAN Interrupt upon receiving a CAN message */
	EnableCANRxISR();

	/* Initializes the Mayor device and sends a CAN message with its city ID */
	ckInit(ckStartDefault);

	while(1){

	}
}