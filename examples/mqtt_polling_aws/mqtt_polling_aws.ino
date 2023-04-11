/**
 * This example connects to the device specific endpoint in AWS in order to
 * publish and retrieve MQTT messages.
 */

#include <Arduino.h>

#include <ecc608.h>
#include <led_ctrl.h>
#include <log.h>
#include <lte.h>
#include <mqtt_client.h>

// AWS defines which topic you are allowed to subscribe and publish too. This is
// defined by the policy The default policy with the Microchip IoT Provisioning
// Tool allows for publishing and subscribing on thing_id/topic. If you want to
// publish and subscribe on other topics, see the AWS IoT Core Policy
// documentation.
#define MQTT_SUB_TOPIC_FMT "%s/sensors"
#define MQTT_PUB_TOPIC_FMT "%s/sensors"

char mqtt_sub_topic[128];
char mqtt_pub_topic[128];

bool initMQTTTopics() {
    ECC608.begin();

    // Find the thing ID and set the publish and subscription topics
    uint8_t thingName[128];
    size_t thingNameLen = sizeof(thingName);

    // -- Get the thingname
    ATCA_STATUS status = ECC608.getThingName(thingName, &thingNameLen);
    if (status != ATCA_SUCCESS) {
        Log.error("Could not retrieve thingname from the ECC");
        return false;
    }

    sprintf(mqtt_sub_topic, MQTT_SUB_TOPIC_FMT, thingName);
    sprintf(mqtt_pub_topic, MQTT_PUB_TOPIC_FMT, thingName);

    return true;
}

void setup() {
    Log.begin(115200);
    LedCtrl.begin();
    LedCtrl.startupCycle();

    Log.info("Starting MQTT Polling for AWS example\r\n");

    if (initMQTTTopics() == false) {
        Log.error("Unable to initialize the MQTT topics. Stopping...");
        while (1) {}
    }

    if (!Lte.begin()) {
        Log.error("Failed to connect to operator");

        // Halt here
        while (1) {}
    }

    // Attempt to connect to the broker
    if (MqttClient.beginAWS()) {
        Log.infof("Connecting to broker");

        while (!MqttClient.isConnected()) {
            Log.rawf(".");
            delay(500);
        }

        Log.rawf(" OK!\r\n");

        // Subscribe to the topic
        MqttClient.subscribe(mqtt_sub_topic);
    } else {
        Log.rawf("\r\n");
        Log.error("Failed to connect to broker");

        // Halt here
        while (1) {}
    }

    // Test MQTT publish and receive
    for (uint8_t i = 0; i < 3; i++) {
        bool published_successfully =
            MqttClient.publish(mqtt_pub_topic, "{\"light\": 9, \"temp\": 9}");

        if (published_successfully) {
            Log.info("Published message");
        } else {
            Log.error("Failed to publish\r\n");
        }

        delay(2000);

        String message = MqttClient.readMessage(mqtt_sub_topic);

        // Read message will return an empty string if there were no new
        // messages, so anything other than that means that there were a
        // new message
        if (message != "") {
            Log.infof("Got new message: %s\r\n", message.c_str());
        }

        delay(2000);
    }

    Log.info("Closing MQTT connection");
    MqttClient.end();
}

void loop() {}
