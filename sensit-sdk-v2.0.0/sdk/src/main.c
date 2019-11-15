/*!******************************************************************
 * \file main.c
 * \brief Sens'it SDK template
 * \author Sens'it Team
 * \copyright Copyright (c) 2018 Sigfox, All Rights Reserved.
 *
 * This file is an empty main template.
 * You can use it as a basis to develop your own firmware.
 *******************************************************************/
/******* INCLUDES **************************************************/
#include "sensit_types.h"
#include "sensit_api.h"
#include "error.h"
#include "button.h"
#include "battery.h"
#include "radio_api.h"
#include "hts221.h"
#include "ltr329.h"
#include "fxos8700.h"
#include "discovery.h"

/******** DEFINES **************************************************/
#define VIBRATION_THRESHOLD                0x10 /* With 2g range, 3,9 mg threshold */
#define VIBRATION_COUNT                    2
/******* GLOBAL VARIABLES ******************************************/
typedef struct {
  u8 EVENT_ID : 4;
  u8 AX : 4;
  u8 AY : 4;
  u8 AZ : 4;
} data_s;

u8 firmware_version[] = "GUILLEZ VPP";

/*******************************************************************/

int main()
{
    error_t err;
    button_e btn;
    u16 battery_level;
    bool send = FALSE;

    /* Discovery payload variable */
    discovery_data_s data = {0};
    discovery_payload_s payload;

    /* Start of initialization */

    /* Configure button */
    SENSIT_API_configure_button(INTERRUPT_BOTH_EGDE);

    /* Initialize Sens'it radio */
    err = RADIO_API_init();
    ERROR_parser(err);

    /* Initialize temperature & humidity sensor */
    err = HTS221_init();
    ERROR_parser(err);

    /* Initialize light sensor */
    err = LTR329_init();
    ERROR_parser(err);

    /* Initialize accelerometer */
    err = FXOS8700_init();
    ERROR_parser(err);

    /* Put accelerometer in transient mode */
    FXOS8700_set_transient_mode(FXOS8700_RANGE_2G, VIBRATION_THRESHOLD, VIBRATION_COUNT);

    /* Clear pending interrupt */
    pending_interrupt = 0;

    /* End of initialization */

    while (TRUE)
    {
        /* Execution loop */

        /* Check of battery level */
        BATTERY_handler(&battery_level);

        /* RTC alarm interrupt handler */
        if ((pending_interrupt & INTERRUPT_MASK_RTC) == INTERRUPT_MASK_RTC)
        {
            /* Clear interrupt */
            pending_interrupt &= ~INTERRUPT_MASK_RTC;
        }

        /* Button interrupt handler */
        if ((pending_interrupt & INTERRUPT_MASK_BUTTON) == INTERRUPT_MASK_BUTTON)
        {
            /* RGB Led ON during count of button presses */
            SENSIT_API_set_rgb_led(RGB_WHITE);

            /* Count number of presses */
            btn = BUTTON_handler();

            /* RGB Led OFF */
            SENSIT_API_set_rgb_led(RGB_OFF);

            if (btn == BUTTON_TWO_PRESSES)
            {
                /* Force a RTC alarm interrupt to do a new measurement */
                pending_interrupt |= INTERRUPT_MASK_RTC;

                /* Set send Sigfox */
                send = TRUE;
            }
            else if (btn == BUTTON_FOUR_PRESSES)
            {
                /* Reset the device */
                SENSIT_API_reset();
            }

            /* Clear interrupt */
            pending_interrupt &= ~INTERRUPT_MASK_BUTTON;
        }

        /* Reed switch interrupt handler */
        if ((pending_interrupt & INTERRUPT_MASK_REED_SWITCH) == INTERRUPT_MASK_REED_SWITCH)
        {
            /* Clear interrupt */
            pending_interrupt &= ~INTERRUPT_MASK_REED_SWITCH;
        }

        /* Accelerometer interrupt handler */
        if ((pending_interrupt & INTERRUPT_MASK_FXOS8700) == INTERRUPT_MASK_FXOS8700)
        {
            /* Read transient interrupt register */
            FXOS8700_clear_transient_interrupt(&(data.vibration));
            /* Check if a movement has been detected */
            if (data.vibration == TRUE)
            {
                /* Set send message flag */
                send = TRUE;
                /* Increment event counter */
                data.event_counter++;
                FXOS8700_read_acceleration(&data);

            }

            /* Clear interrupt */
            pending_interrupt &= ~INTERRUPT_MASK_FXOS8700;
        }

        /* Check if we need to send a message */
        if (send == TRUE)
        {
            /* Build the payload */
            DISCOVERY_build_payload(&payload, MODE_VIBRATION, &data);

            /* Send the message */
            err = RADIO_API_send_message(RGB_BLUE, (u8*)&payload, DISCOVERY_PAYLOAD_SIZE, FALSE, NULL);
            /* Parse the error code */
            ERROR_parser(err);

            if (err == RADIO_ERR_NONE)
            {
                /* Reset event counter */
                data.event_counter = 0;
            }

            /* Clear vibration flag */
            data.vibration = FALSE;

            /* Clear button flag */
            data.button = FALSE;

            /* Clear send flag */
            send = FALSE;
        }

        /* Check if all interrupt have been clear */
        if (pending_interrupt == 0)
        {
            /* Wait for Interrupt */
            SENSIT_API_sleep(FALSE);
        }
    } /* End of while */
}

/*******************************************************************/
