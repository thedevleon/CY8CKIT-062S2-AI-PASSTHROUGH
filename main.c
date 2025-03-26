/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for Hello World Example using HAL APIs.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2022-2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"


/*******************************************************************************
* Macros
*******************************************************************************/
/* LED blink timer clock value in Hz  */
#define LED_BLINK_TIMER_CLOCK_HZ          (10000)

/* LED blink timer period value */
#define LED_BLINK_TIMER_PERIOD            (9999)

// Mapping of RSPI pins to P9 pins
// RSPI_MOSI -> P9.7, RSPI_MISO -> P9.6, RSPI_CLK -> P9.5, RSPI_CS -> P9.4, RSPI_IRQ -> P9.3, RXRES_L -> P9.2
// TODO: PDM_CLK, PDM_DATA, and interrupts for other sensors
#define EXT_SPI_MOSI P9_7
#define EXT_SPI_MISO P9_6
#define EXT_SPI_CLK P9_5
#define EXT_SPI_CS P9_4
#define EXT_SPI_IRQ P9_3
#define EXT_RXRES_L P9_2



/*******************************************************************************
* Global Variables
*******************************************************************************/
bool timer_interrupt_flag = false;
bool led_blink_active_flag = true;

// Interrupts for SPI_CLK and SPI_IRQ
cyhal_gpio_callback_data_t gpio_spi_clk_callback_data;
cyhal_gpio_callback_data_t gpio_spi_irq_callback_data;

/* Timer object used for blinking the LED */
cyhal_timer_t led_blink_timer;


/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void timer_init(void);
static void isr_timer(void *callback_arg, cyhal_timer_event_t event);
static void gpio_spi_clk_interrupt_handler(void *handler_arg, cyhal_gpio_event_t event);
static void gpio_spi_irq_interrupt_handler(void *handler_arg, cyhal_gpio_event_t event);

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function. It sets up a timer to trigger a periodic interrupt.
* The main while loop checks for the status of a flag set by the interrupt and
* toggles an LED at 1Hz to create an LED blinky. Will be achieving the 1Hz Blink
* rate based on the The LED_BLINK_TIMER_CLOCK_HZ and LED_BLINK_TIMER_PERIOD
* Macros,i.e. (LED_BLINK_TIMER_PERIOD + 1) / LED_BLINK_TIMER_CLOCK_HZ = X ,Here,
* X denotes the desired blink rate. The while loop also checks whether the
* 'Enter' key was pressed and stops/restarts LED blinking.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

#if defined (CY_DEVICE_SECURE)
    cyhal_wdt_t wdt_obj;

    /* Clear watchdog timer so that it doesn't trigger a reset */
    result = cyhal_wdt_init(&wdt_obj, cyhal_wdt_get_max_timeout_ms());
    CY_ASSERT(CY_RSLT_SUCCESS == result);
    cyhal_wdt_free(&wdt_obj);
#endif /* #if defined (CY_DEVICE_SECURE) */

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init_fc(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
            CYBSP_DEBUG_UART_CTS,CYBSP_DEBUG_UART_RTS,CY_RETARGET_IO_BAUDRATE);

    /* retarget-io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    // printf("\x1b[2J\x1b[;H");

    /* Initialize the User LED */
    result = cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT,
                             CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    // /* Initialize all pins for the Radar
    // CYBSP_RSPI_MOSI, CYBSP_RSPI_MISO, CYBSP_RSPI_CLK, CYBSP_RSPI_CS, CYBSP_RSPI_IRQ (interrupt), CYBSP_RXRES_L (reset)
    // */
    result = cyhal_gpio_init(CYBSP_RSPI_MOSI, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
    result = cyhal_gpio_init(CYBSP_RSPI_MISO, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, false);
    result = cyhal_gpio_init(CYBSP_RSPI_CLK, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
    result = cyhal_gpio_init(CYBSP_RSPI_CS, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);
    result = cyhal_gpio_init(CYBSP_RSPI_IRQ, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, false);
    result = cyhal_gpio_init(CYBSP_RXRES_L, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);

    /* Initialize all pins for the Passthrough (P9.7 to P9.0)
    RSPI_MOSI -> P9.7, RSPI_MISO -> P9.6, RSPI_CLK -> P9.5, RSPI_CS -> P9.4, RSPI_IRQ -> P9.3, RXRES_L -> P9.2
    // TODO: PDM_CLK, PDM_DATA, and interrupts for other sensors
    */
    result = cyhal_gpio_init(EXT_SPI_MOSI, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, false);
    result = cyhal_gpio_init(EXT_SPI_MISO, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
    result = cyhal_gpio_init(EXT_SPI_CLK, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, false);
    result = cyhal_gpio_init(EXT_SPI_CS, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, true);
    result = cyhal_gpio_init(EXT_SPI_IRQ, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
    result = cyhal_gpio_init(EXT_RXRES_L, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, false);

    /* GPIO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    { 
        printf("GPIO init failed \r\n");
        printf("Error code: %lu \r\n", result);
        CY_ASSERT(0);
    }

    // Setup interrupts for SPI_CLK and  SPI_IRQ
    gpio_spi_clk_callback_data.callback = gpio_spi_clk_interrupt_handler;
    gpio_spi_irq_callback_data.callback = gpio_spi_irq_interrupt_handler;

    cyhal_gpio_register_callback(EXT_SPI_CLK, &gpio_spi_clk_callback_data);
    cyhal_gpio_register_callback(CYBSP_RSPI_IRQ, &gpio_spi_irq_callback_data);

    // TODO which one should have higher priority?
    cyhal_gpio_enable_event(EXT_SPI_CLK, CYHAL_GPIO_IRQ_BOTH, 7, true);
    cyhal_gpio_enable_event(CYBSP_RSPI_IRQ, CYHAL_GPIO_IRQ_BOTH, 7, true);

    printf("****************** "
           "Radar Passthrough v0.0.1"
           "****************** \r\n\n");

    /* Initialize timer to toggle the LED */
    timer_init();

    for (;;)
    {   
        /* Check if timer elapsed (interrupt fired) and toggle the LED */
        if (timer_interrupt_flag)
        {
            /* Clear the flag */
            timer_interrupt_flag = false;

            /* Invert the USER LED state */
            cyhal_gpio_toggle(CYBSP_USER_LED);
        }
    }
}


/*******************************************************************************
* Function Name: timer_init
********************************************************************************
* Summary:
* This function creates and configures a Timer object. The timer ticks
* continuously and produces a periodic interrupt on every terminal count
* event. The period is defined by the 'period' and 'compare_value' of the
* timer configuration structure 'led_blink_timer_cfg'. Without any changes,
* this application is designed to produce an interrupt every 1 second.
*
* Parameters:
*  none
*
* Return :
*  void
*
*******************************************************************************/
 void timer_init(void)
 {
    cy_rslt_t result;

    const cyhal_timer_cfg_t led_blink_timer_cfg =
    {
        .compare_value = 0,                 /* Timer compare value, not used */
        .period = LED_BLINK_TIMER_PERIOD,   /* Defines the timer period */
        .direction = CYHAL_TIMER_DIR_UP,    /* Timer counts up */
        .is_compare = false,                /* Don't use compare mode */
        .is_continuous = true,              /* Run timer indefinitely */
        .value = 0                          /* Initial value of counter */
    };

    /* Initialize the timer object. Does not use input pin ('pin' is NC) and
     * does not use a pre-configured clock source ('clk' is NULL). */
    result = cyhal_timer_init(&led_blink_timer, NC, NULL);

    /* timer init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Configure timer period and operation mode such as count direction,
       duration */
    cyhal_timer_configure(&led_blink_timer, &led_blink_timer_cfg);

    /* Set the frequency of timer's clock source */
    cyhal_timer_set_frequency(&led_blink_timer, LED_BLINK_TIMER_CLOCK_HZ);

    /* Assign the ISR to execute on timer interrupt */
    cyhal_timer_register_callback(&led_blink_timer, isr_timer, NULL);

    /* Set the event on which timer interrupt occurs and enable it */
    cyhal_timer_enable_event(&led_blink_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT,
                              7, true);

    /* Start the timer with the configured settings */
    cyhal_timer_start(&led_blink_timer);
 }


/*******************************************************************************
* Function Name: isr_timer
********************************************************************************
* Summary:
* This is the interrupt handler function for the timer interrupt.
*
* Parameters:
*    callback_arg    Arguments passed to the interrupt callback
*    event            Timer/counter interrupt triggers
*
* Return:
*  void
*******************************************************************************/
static void isr_timer(void *callback_arg, cyhal_timer_event_t event)
{
    (void) callback_arg;
    (void) event;

    /* Set the interrupt flag and process it from the main while(1) loop */
    timer_interrupt_flag = true;
}

static void gpio_spi_clk_interrupt_handler(void *handler_arg, cyhal_gpio_event_t event)
{
    (void) handler_arg;
    (void) event;

    bool signal = event == CYHAL_GPIO_IRQ_RISE ? true : false;

    printf("SPI_CLK interrupt triggered: %d \r\n", signal);

    // Passthrough the SPI signals
    cyhal_gpio_write(CYBSP_RSPI_MOSI, cyhal_gpio_read(EXT_SPI_MOSI));
    cyhal_gpio_write(EXT_SPI_MISO, cyhal_gpio_read(CYBSP_RSPI_MISO));
    cyhal_gpio_write(CYBSP_RSPI_CLK, signal);
    cyhal_gpio_write(CYBSP_RSPI_CS, cyhal_gpio_read(EXT_SPI_CS));
    cyhal_gpio_write(EXT_RXRES_L, cyhal_gpio_read(CYBSP_RXRES_L));
}

static void gpio_spi_irq_interrupt_handler(void *handler_arg, cyhal_gpio_event_t event)
{
    (void) handler_arg;
    (void) event;

    printf("SPI_IRQ interrupt triggered \r\n");
    cyhal_gpio_write(EXT_SPI_IRQ, cyhal_gpio_read(CYBSP_RSPI_IRQ));
}

/* [] END OF FILE */
