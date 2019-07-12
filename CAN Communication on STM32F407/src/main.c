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

struct s_canmsg cmsg;
bool xmsgidf, rtrf;
int *ap, led_blink_count;
BaseType_t xReturned;
TaskHandle_t xHandle = NULL;


void Controller_Blink_Task(void  *pvParameters);
void Worker_Blink_Task(void  *pvParamters);
void Led_Blink_Count_Task(void *pvParameters);

/* Set STM32 to 168 MHz. */
static void clock_setup(void)
{
	rcc_clock_setup_hse_3v3(&hse_8mhz_3v3[CLOCK_3V3_168MHZ]);		
}

static void button_setup(void)
{
	/* Enable GPIOA clock. */
	rcc_periph_clock_enable(RCC_GPIOA);

	/* Set GPIOA0 to 'input floating'. */
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO0);	
}

void receive(uint8_t fifo){

    can_receive(CAN1,
                fifo, // FIFO id
                true, // Automatically release FIFO after rx
                &cmsg.msgid, &xmsgidf, &rtrf, (uint32_t *) &cmsg.fmi,
                &cmsg.length, cmsg.data);

	cmsg.xmsgidf = xmsgidf;
	cmsg.rtrf = rtrf;
	cmsg.fifo = fifo;
	
}

//void can1_rx0_isr(void){
//	gpio_toggle(GPIOD, GPIO14);
// 	if (CAN_RF0R(CAN1) & CAN_RF0R_FMP0_MASK) {
//         receive(0);	   
//     }

//     // Message pending on FIFO 1?
//     if (CAN_RF1R(CAN1) & CAN_RF1R_FMP1_MASK) {
//         receive(1);
//     }
// 	int delayValue = 0;
// 	for(int i = 0; i < cmsg.length; i++){
// 		delayValue = delayValue + cmsg.data[i];
// 	}

// 	gpio_set(GPIOD, GPIO13);
// 	for(long long i = 0; i < 9999999999 * delayValue; i++){

// 	}
// 	gpio_clear(GPIOD, GPIO13);
// }

void can1_tx_isr(void){
	gpio_toggle(GPIOD, GPIO15);

	/* Currently disabled because CAN request is being sent periodically. Enable if CAN Tx request interrupt is required. 
	Use this primarily when there are FreeRTOS tasks involved. Remember: Tasks don't execute until interrupts are finished executing. */
	//nvic_disable_irq(NVIC_CAN1_TX_IRQ);
	//can_disable_irq(CAN1, NVIC_CAN1_TX_IRQ);
}

static void exti_setup(void)
{
	/* Enable GPIOA clock. */
	rcc_periph_clock_enable(RCC_GPIOA);
	//rcc_periph_clock_enable(RCC_APB1ENR_TIM2EN);

	/* Enable EXTI0 interrupt. */
	nvic_enable_irq(NVIC_EXTI0_IRQ);

	/* Set GPIO0 (in GPIO port A) to 'input float'. */
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO0);

	/* Configure the EXTI subsystem. */
	exti_select_source(EXTI0, GPIOA);
	exti_set_trigger(EXTI0, EXTI_TRIGGER_FALLING);
	exti_enable_request(EXTI0);
}

void exti0_isr(void)
{
	int messageID = 0;
	int messageData = 0;

	messageID = rand() % 100 + 1;
	messageData = rand() % 100 + 1;
	ap = &messageData;	

	can_transmit(CAN1, messageID, false, false, 1, (void *) ap);
	
	exti_reset_request(EXTI0);
}

void Create_Blinking_Task(void){
	xTaskCreate(&Worker_Blink_Task, (const char* const) "Worker Blink Task", configMINIMAL_STACK_SIZE, NULL, 2, 0);
}

void Create_Controller_Task(void){
	xReturned = xTaskCreate(&Controller_Blink_Task, (const char* const) "Controller Blink Task", configMINIMAL_STACK_SIZE, NULL, 2, &xHandle);
}

void Create_Led_Blink_Count_Task(void){
	xTaskCreate(&Led_Blink_Count_Task, (const char* const) "LED Blink Count Task", configMINIMAL_STACK_SIZE, NULL, 1, 0);
}

void Controller_Blink_Task(void *pvParameters){	
	int messageID = 0;
	int messageData = 0;
	for(;;){

	messageID = 123;
	messageData = 5;
	ap = &messageData;	
	

	can_transmit(CAN1, messageID, false, false, 1, (void *) &messageData);
	gpio_toggle(GPIOD, GPIO15);	
	/* Currently disabled because CAN request is being sent periodically. Enable if CAN Tx request interrupt is required */
	//nvic_enable_irq(NVIC_CAN1_TX_IRQ);
	//can_enable_irq(CAN1, NVIC_CAN1_TX_IRQ);

	vTaskDelay(6000);
	}
}

void Led_Blink_Count_Task(void *pvParameters){
	int messageID = 0;
	int messageData = 0;
	for(;;){
		messageID = 456;
		messageData = led_blink_count;

		can_transmit(CAN1, messageID, false, false, 1, (void *) &messageData);
		vTaskDelay(20000);		
	}
}

void Worker_Blink_Task(void *pvParameters){	
	for(;;) 
	{
		int delayValue = 0;
		cmsg.length = 0;
		int totalDelay = 0;
		if (CAN_RF0R(CAN1) & CAN_RF0R_FMP0_MASK) 
		{		
        	receive(0);	   
    	}

    	// Message pending on FIFO 1?
    	if (CAN_RF1R(CAN1) & CAN_RF1R_FMP1_MASK) 
		{		
        	receive(1);
    	}
		if(cmsg.length != 0){
				for(int i = 0; i <= cmsg.length; i++){
				delayValue = delayValue + cmsg.data[i];
			}				
		}
		if(delayValue != 0)
		{
			totalDelay = delayValue * 1000;
			if(cmsg.msgid == 123){
				gpio_set(GPIOD, GPIO13);
				vTaskDelay(totalDelay);
				gpio_clear(GPIOD, GPIO13);
				led_blink_count++;			
			}
			if(cmsg.msgid == 456){
				gpio_set(GPIOD, GPIO12);
				vTaskDelay(totalDelay);
				gpio_clear(GPIOD, GPIO12);				
			}			
		}	
	}
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

	// nvic_enable_irq(NVIC_CAN1_RX0_IRQ);
	 
	 /* Uncomment for CAN Tx interrupt*/
	// nvic_enable_irq(NVIC_CAN1_TX_IRQ);
	
	//can_enable_irq(CAN1, NVIC_CAN1_RX0_IRQ);
	
	/* Uncomment for CAN Tx interrupt */
	//can_enable_irq(CAN1, NVIC_CAN1_TX_IRQ);

	//nvic_set_priority(NVIC_CAN1_RX0_IRQ, 0);
		
	// CAN transmission rate of 250 Kbps
	// can_init(CAN1,
	// false,
	// true,
	// true,
	// false,
	// false,
	// false,
	// CAN_BTR_SJW_1TQ,
	// CAN_BTR_TS1_6TQ,
	// CAN_BTR_TS2_1TQ,
	// 21,
	// false,
	// false	
	// );	

	// Transmission speed of 1 Mbps
	// can_init(CAN1,
	// false,
	// true,
	// true,
	// false,
	// false,
	// false,
	// CAN_BTR_SJW_1TQ,
	// CAN_BTR_TS1_6TQ,
	// CAN_BTR_TS2_7TQ,
	// 3,
	// false,
	// false	
	// );

	can_init(CAN1,
	false,
	true,
	true,
	false,
	false,
	false,
	CAN_BTR_SJW_1TQ,
	CAN_BTR_TS1_13TQ,
	CAN_BTR_TS2_2TQ,
	21,
	false,
	false	
	);		

	const uint16_t id1 = (
    (123 << 5)  // STDID
	);

	const u_int16_t mask1 = (
		(0b11111111111 << 5)
	);

	const uint16_t id2 = (
		(456 << 5)
	);
	
	/* Enable this filter and disable all other filters to accept all messages */
	// can_filter_id_mask_32bit_init(CAN1,
	// 			0,     /* Filter ID */
	// 			0,     /* CAN ID */
	// 			0,     /* CAN ID mask */
	// 			0,     /* FIFO assignment (here: FIFO0) */
	// 			true); /* Enable the filter. */

	can_filter_id_mask_16bit_init(CAN1,
	0,
	id1,
	mask1,
	id2,
	mask1,
	0,
	true);
					

	can_enable_irq(CAN1, CAN_IER_FMPIE0);
	can_enable_irq(CAN1, CAN_IER_FMPIE1);		

	clock_setup();
	button_setup();	
	exti_setup();

	/* Enable the Create_Controller_Task to transmit messages and 
	Enable the Creat_Blinking_Task to receive messages */
	//Create_Controller_Task();
	Create_Blinking_Task();
	Create_Led_Blink_Count_Task();
	vTaskStartScheduler();

	while(1){

	}
}