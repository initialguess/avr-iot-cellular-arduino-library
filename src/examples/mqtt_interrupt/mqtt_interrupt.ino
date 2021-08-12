/**
 * This example uses callbacks for LTE and MQTT.
 */

#include <Arduino.h>
#include <lte.h>
#include <mqtt_client.h>
#include <sequans_controller.h>

#define MQTT_THING_NAME "cccf626c3be836af9f72fb534b42b3ea4cc6e1dd"
#define MQTT_BROKER     "a1gqt8sttiign3-ats.iot.us-east-2.amazonaws.com"
#define MQTT_PORT       8883
#define MQTT_USE_TLS    true
#define MQTT_USE_ECC    false

#define SerialDebug Serial5

#define CELL_LED       PIN_PG2
#define CONNECTION_LED PIN_PG3

#define NETWORK_CONN_FLAG    1 << 0
#define NETWORK_DISCONN_FLAG 1 << 1
#define BROKER_CONN_FLAG     1 << 2
#define BROKER_DISCONN_FLAG  1 << 3
#define RECEIVE_MSG_FLAG     1 << 4

typedef enum { NOT_CONNECTED, CONNECTED_TO_NETWORK, CONNECTED_TO_BROKER } State;

State state;
uint8_t callback_flags = 0;

// -------------------------- CALLBACKS & SETUP ---------------------------- //

void connectedToNetwork(void) { callback_flags |= NETWORK_CONN_FLAG; }
void disconnectedFromNetwork(void) { callback_flags |= NETWORK_DISCONN_FLAG; }

void connectedToBroker(void) { callback_flags |= BROKER_CONN_FLAG; }
void disconnectedFromBroker(void) { callback_flags |= BROKER_DISCONN_FLAG; }

void receive(void) { callback_flags |= RECEIVE_MSG_FLAG; }

void setup() {
    Serial5.begin(115200);
    SerialDebug.println("Starting initialization");

    pinMode(CELL_LED, OUTPUT);
    pinMode(CONNECTION_LED, OUTPUT);

    // These pins is active low
    digitalWrite(CELL_LED, HIGH);
    digitalWrite(CONNECTION_LED, HIGH);

    // Register callbacks for network connection
    Lte.onConnectionStatusChange(connectedToNetwork, disconnectedFromNetwork);
    Lte.begin();
}

// ----------------------------- STATE MACHINE ------------------------------ //

void loop() {

    debugBridgeUpdate();

    if (callback_flags & NETWORK_CONN_FLAG) {
        switch (state) {
        case NOT_CONNECTED:
            state = CONNECTED_TO_NETWORK;
            digitalWrite(CELL_LED, LOW);

            // Attempt connection to MQTT broker
            MqttClient.onConnectionStatusChange(connectedToBroker,
                                                disconnectedFromBroker);
            MqttClient.onReceive(receive);

            if (!MqttClient.begin(MQTT_THING_NAME,
                                  MQTT_BROKER,
                                  MQTT_PORT,
                                  MQTT_USE_TLS,
                                  MQTT_USE_ECC)) {
                SerialDebug.println("Failed to connect to broker");
            }

            break;
        }

        callback_flags &= ~NETWORK_CONN_FLAG;

    } else if (callback_flags & NETWORK_DISCONN_FLAG) {
        switch (state) {
        default:
            MqttClient.end();
            state = NOT_CONNECTED;
            digitalWrite(CONNECTION_LED, HIGH);
            digitalWrite(CELL_LED, HIGH);
            break;
        }

        callback_flags &= ~NETWORK_DISCONN_FLAG;

    } else if (callback_flags & BROKER_CONN_FLAG) {
        switch (state) {

        case CONNECTED_TO_NETWORK:
            state = CONNECTED_TO_BROKER;
            digitalWrite(CONNECTION_LED, LOW);
            break;
        }

        callback_flags &= ~BROKER_CONN_FLAG;

    } else if (callback_flags & BROKER_DISCONN_FLAG) {

        switch (state) {

        case CONNECTED_TO_BROKER:
            state = CONNECTED_TO_NETWORK;
            digitalWrite(CONNECTION_LED, LOW);
            break;
        }

        callback_flags &= ~BROKER_DISCONN_FLAG;

    } else if (callback_flags & RECEIVE_MSG_FLAG) {

        switch (state) {
        case CONNECTED_TO_BROKER:

            MqttReceiveNotification notification =
                MqttClient.readReceiveNotification();

            // Failed to read notification or some error happened
            if (notification.message_length == 0) {
                return;
            }

            // Extra space for termination
            char buffer[notification.message_length + 16] = "";

            if (MqttClient.readMessage(notification.receive_topic.c_str(),
                                       buffer,
                                       sizeof(buffer))) {
                Serial5.printf("I got the messsage: %s\r\n", (char *)buffer);

                // We publish a message back
                publishMessage("tobroker", buffer);
            } else {
                Serial5.printf("Failed to read message\r\n");
            }

            break;
        }

        callback_flags &= ~RECEIVE_MSG_FLAG;
    }
}

// ------------------------------ DEBUGBRIDGE ------------------------------ //

#define DEL_CHARACTER   127
#define ENTER_CHARACTER 13

#define INPUT_BUFFER_SIZE    128
#define RESPONSE_BUFFER_SIZE 256

void debugBridgeUpdate(void) {
    static uint8_t character;
    static char input_buffer[INPUT_BUFFER_SIZE];
    static uint8_t input_buffer_index = 0;

    if (Serial5.available() > 0) {
        character = Serial5.read();

        switch (character) {
        case DEL_CHARACTER:
            if (strlen(input_buffer) > 0) {
                input_buffer[input_buffer_index--] = 0;
            }
            break;

        case ENTER_CHARACTER:

            SequansController.writeCommand(input_buffer);

            // Reset buffer
            memset(input_buffer, 0, sizeof(input_buffer));
            input_buffer_index = 0;

            break;

        default:
            input_buffer[input_buffer_index++] = character;
            break;
        }

        Serial5.print((char)character);
    }

    if (SequansController.isRxReady()) {
        // Send back data from modem to host
        Serial5.write(SequansController.readByte());
    }
}
