#include "sequans_controller.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stddef.h>
#include <string.h>
#include <util/delay.h>

#include <Arduino.h>
#include <pins_arduino.h>

#define TX_PIN      PIN_PC0
#define CTS_PIN     PIN_PC6
#define CTS_PIN_bm  PIN6_bm
#define CTS_INT_bm  PORT_INT6_bm
#define RING_INT_bm PORT_INT4_bm
#define RTS_PIN     PIN_PC7
#define RESET_PIN   PIN_PE1
#define RING_PIN    PIN_PC4

#define SerialAT                 Serial1
#define HWSERIALAT               USART1
#define SEQUANS_MODULE_BAUD_RATE 115200
#define RX_BUFFER_ALMOST_FULL    SERIAL_RX_BUFFER_SIZE - 2

#define LINE_FEED            0xA
#define CARRIAGE_RETURN      0xD
#define RESPONSE_DELIMITER   ","
#define DATA_START_CHARACTER ':'
#define SPACE_CHARACTER      ' '

#define SEQUANS_CONTROLLER_DEFAULT_RETRIES        5
#define SEQUANS_CONTROLLER_DEFAULT_RETRY_SLEEP_MS 10

static const char OK_TERMINATION[] = "OK\r\n";
static const char ERROR_TERMINATION[] = "ERROR\r\n";

static char URC_BUFFER[64];

static uint8_t number_of_retries = SEQUANS_CONTROLLER_DEFAULT_RETRIES;
static double sleep_between_retries_ms =
    SEQUANS_CONTROLLER_DEFAULT_RETRY_SLEEP_MS;

/** @brief Flow control update for the UART interface with the LTE modules
 *
 * Updates RTS line based on space available in receive buffer. If the buffer
 * is close to full the RTS line is de-asserted (set high) to signal to the
 * target that no more data should be sent
 */
static void flowControlUpdate(void) {

    if (SerialAT.available() < RX_BUFFER_ALMOST_FULL) {
        // Space for more data, assert RTS line (active low)
        digitalWrite(RTS_PIN, LOW);
    } else {
        // Buffer is filling up, tell the target to stop sending data
        // for now by de-asserting RTS
        digitalWrite(RTS_PIN, HIGH);
    }
}

ISR(PORTC_PORT_vect) {
    if (VPORTC.INTFLAGS & CTS_INT_bm) {

        if (VPORTC.IN & CTS_PIN_bm) {
            // CTS is not asserted (active low) so disable USART Data Register
            // Empty Interrupt where the logic is to send more data
            HWSERIALAT.CTRLA &= ~(1 << USART_DREIE_bp);
        } else {
            // CTS is asserted check if there is data to transmit
            // before we enable interrupt
            HWSERIALAT.CTRLA |= (1 << USART_DREIE_bp);
        }

        /*
        else if (VPORTC.INTFLAGS & RING_INT_bm) {

            // TODO: Makes the ISR quite fat :/
            // TODO: Read URC
            Serial5.print("Received URC: ");


        }
        */
    }

    VPORTC.INTFLAGS = 0xff;
}

/* TODO: Need to figure out a way to incorporate this into SerialAT
// RX complete
ISR(USART1_RXC_vect) {
    flowControlUpdate();

    // TODO: would want to do this and then call the interrupt handler in
    // SerialAT
    // _rx_complete_irq
}
*/

void sequansControllerBegin(void) {

    // PIN SETUP

    pinConfigure(TX_PIN, PIN_DIR_OUTPUT | PIN_PULLUP_ON);

    // Request to send (RTS) and clear to send (CTS) are the control lines
    // on the UART line. From the configuration the MCU and the LTE modem is
    // in, we control the RTS line from the MCU to signalize if we can process
    // more data or not from the LTE modem. The CTS line is controlled from
    // the LTE modem and gives us the ability to know whether the LTE modem
    // can receive more data or if we have to wait.
    //
    // Both pins are active low.

    // We assert RTS high until we are ready to receive more data
    pinConfigure(RTS_PIN, PIN_DIR_OUTPUT);
    digitalWrite(RTS_PIN, HIGH);

    // Clear to send is input and we want interrupts on both edges to know
    // when the LTE modem has changed the state of the line.
    pinConfigure(CTS_PIN, PIN_DIR_INPUT | PIN_PULLUP_ON | PIN_INT_CHANGE);

    // Set reset low to reset the LTE modem
    pinConfigure(RESET_PIN, PIN_DIR_OUTPUT);
    digitalWrite(RESET_PIN, LOW);

    // We want to set up an interrupt on the RING0 pin which will be triggered
    // when we receive an URC from the modem
    //
    // The polarity of it is inverted, so we add an interrupt on falling edge
    // TODO: uncomment
    // pinConfigure(RING_PIN, PIN_DIR_INPUT | PIN_INT_FALL);

    // SERIAL INTERFACE SETUP

    SerialAT.begin(SEQUANS_MODULE_BAUD_RATE);

    flowControlUpdate();
}

void sequansControllerEnd(void) {
    SerialAT.end();
    pinConfigure(CTS_PIN, 0);
}

void sequansControllerSetRetryConfiguration(const uint8_t num_retries,
                                            const double sleep_ms) {

    number_of_retries = num_retries;
    sleep_between_retries_ms = sleep_ms;
}

bool sequansControllerIsTxReady(void) { return SerialAT.availableForWrite(); }

bool sequansControllerIsRxReady(void) { return SerialAT.available() > 0; }

bool sequansControllerWriteByte(const uint8_t data) {

    uint8_t retry_count = 0;
    while (!sequansControllerIsTxReady()) {
        retry_count++;

        if (retry_count == number_of_retries) {
            return false;
        }

        _delay_ms(sleep_between_retries_ms);
    }

    SerialAT.write(data);

    return true;
}

bool sequansControllerWriteCommand(const char *command) {
    const size_t length = strlen(command);

    for (size_t i = 0; i < length; i++) {
        if (!sequansControllerWriteByte(command[i])) {
            return false;
        }
    }

    return sequansControllerWriteByte('\r');
}

bool sequansControllerWriteBytes(const uint8_t *data,
                                 const size_t buffer_size) {

    for (size_t i = 0; i < buffer_size; i++) {
        if (!sequansControllerWriteByte(data[i])) {
            return false;
        }
    }

    return sequansControllerWriteByte('\r');
}

int16_t sequansControllerReadByte() {
    flowControlUpdate();

    return SerialAT.read();
}

ResponseResult sequansControllerReadResponse(char *out_buffer,
                                             uint16_t buffer_size) {
    uint8_t retry_count = 0;
    int16_t value;

    for (size_t i = 0; i < buffer_size; i++) {
        if (!sequansControllerIsRxReady()) {
            retry_count++;
            delay(sleep_between_retries_ms);

            i--;

            if (retry_count == number_of_retries) {
                return TIMEOUT;
            }

            continue;
        }

        // Reset if we get a valid value
        retry_count = 0;

        out_buffer[i] = (uint8_t)sequansControllerReadByte();

        // For AT command responses from the LTE module, "OK\r\n" or
        // "ERROR\r\n" signifies the end of a response, so we look "\r\n".
        if (i >= 1 && out_buffer[i - 1] == CARRIAGE_RETURN &&
            out_buffer[i] == LINE_FEED) {

            char *ok_termination_index = strstr(out_buffer, OK_TERMINATION);
            if (ok_termination_index != NULL) {

                // We set the rest of the buffer from the "OK\r\n" to 0
                memset(ok_termination_index,
                       0,
                       buffer_size - (ok_termination_index - out_buffer));

                return OK;
            }

            char *error_termination_index =
                strstr(out_buffer, ERROR_TERMINATION);

            if (error_termination_index != NULL) {
                // We set the rest of the buffer from the "ERROR\r\n" to 0
                memset(error_termination_index,
                       0,
                       buffer_size - (error_termination_index - out_buffer));

                return ERROR;
            }
        }
    }

    // Didn't find the end marker within the number of bytes given for the
    // response. Caller should increase the buffer size.
    return BUFFER_OVERFLOW;
}

ResponseResult sequansControllerFlushResponse(void) {

    // We use the termination buffer to look for a "\r\n", which can indicate
    // termination. We set it to max size of the error termination so that we
    // can fit it as well as the 'OK' termination
    char termination_buffer[sizeof(ERROR_TERMINATION)] = "";

    // Fill with non-null values in order for checking for substrings to work
    memset(termination_buffer, ' ', sizeof(termination_buffer) - 1);

    // Don't include null termination
    size_t termination_buffer_size = sizeof(termination_buffer) - 1;

    uint8_t retry_count = 0;

    // We will break out of the loop if we find the termination sequence
    // or if we pass the retry count.
    while (retry_count < number_of_retries) {

        if (!sequansControllerIsRxReady()) {
            retry_count++;
            _delay_ms(sleep_between_retries_ms);
            continue;
        }

        // Shift the buffer backwards
        for (size_t i = 0; i < termination_buffer_size - 1; i++) {
            termination_buffer[i] = termination_buffer[i + 1];
        }

        termination_buffer[termination_buffer_size - 1] =
            sequansControllerReadByte();
        // Reset retry count when we get some data
        retry_count = 0;

        if (termination_buffer[termination_buffer_size - 2] ==
                CARRIAGE_RETURN &&
            termination_buffer[termination_buffer_size - 1] == LINE_FEED) {

            char *ok_termination_index =
                strstr(termination_buffer, OK_TERMINATION);
            if (ok_termination_index != NULL) {
                return OK;
            }

            char *error_termination_index =
                strstr(termination_buffer, ERROR_TERMINATION);

            if (error_termination_index != NULL) {
                return ERROR;
            }
        }
    }

    return TIMEOUT;
}

bool sequansControllerExtractValueFromCommandResponse(
    char *response,
    const uint8_t index,
    char *buffer,
    const size_t buffer_size) {

    // Find the last carriage return (if any)
    char *response_end = strrchr(response, '\r');

    // If not found, set end pointer to last character
    if (response_end == NULL) {
        response_end = response + strlen(response);
    }

    // Using strtok further down in this function would modify the original
    // string, so we create a copy to the end index + 1 (because of
    // NULL termination).
    const size_t response_size = (response_end - response) + 1;
    char response_copy[response_size];
    memcpy(response_copy, response, response_size);

    // Find the first occurrence of the data start character and move pointer to
    // there
    char *data = strchr(response_copy, DATA_START_CHARACTER);

    if (data == NULL) {
        return false;
    }

    // Increment pointer to skip the data start character (and the following
    // space in the start sequence of the data if it is there)
    while (*data == DATA_START_CHARACTER || *data == SPACE_CHARACTER) {
        data++;
    }

    // Now we split the string by the response delimiter and search for the
    // index we're interested in
    char *value = strtok(data, RESPONSE_DELIMITER);

    uint8_t value_index = 1;
    while (value != NULL && value_index <= index) {
        value = strtok(NULL, RESPONSE_DELIMITER);

        value_index++;
    }

    // Remove any carriage return
    char *value_end = strrchr(value, '\r');

    // If not found, set termination to the carriage return
    if (value != NULL) {
        *value_end = 0;
    }

    size_t value_length = strlen(value);

    // We compare inclusive for value length as we want to take the null
    // termination into consideration. So the buffer size has be
    // value_length + 1
    if (value == NULL || value_length >= buffer_size) {
        return false;
    }

    memcpy(buffer, value, value_length);

    return true;
}