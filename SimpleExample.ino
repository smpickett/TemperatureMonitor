#include "SSDP.h"
#include "SparkJson.h"
#include "MQTT.h"
#include "ds18x20.h"
#include "onewire.h"

void log(char* msg);
char logmsg[64];

//-------------------------------------------------------------------------------------------
// == MQTT Setup ==
// Note: Reference from
//    https://www.losant.com/blog/how-to-connect-a-particle-photon-to-the-losant-iot-platform
//
// Note: credentials should not be pushed to github, so they are stored in a separate file.
// Structure of file shoud be:
//     #define LOSANT_BROKER "broker.losant.com"
//     #define LOSANT_DEVICE_ID "my-device-id"
//     #define LOSANT_ACCESS_KEY "my-access-key"
//     #define LOSANT_ACCESS_SECRET "my-access-secret"
#include "LosantCredentials.h"

// Topic used to subscribe to Losant commands.
String MQTT_TOPIC_COMMAND = String::format("losant/%s/command", LOSANT_DEVICE_ID);

// Topic used to publish state to Losant.
String MQTT_TOPIC_STATE = String::format("losant/%s/state", LOSANT_DEVICE_ID);

// Callback signature for MQTT subscriptions.
void callback(char* topic, byte* payload, unsigned int length);

// MQTT client.
MQTT MQTTclient(LOSANT_BROKER, 1883, callback);

// vars
int MQTTlastUpdate = millis();

// functions
void MQTTconnect(void);

//-------------------------------------------------------------------------------------------
// == One Wire Setup ==
#define OW_BUS_PIN  D0
bool read_temperature(float data[], int dataLen);

//-------------------------------------------------------------------------------------------
// == SSDP Setup ==
SSDP ssdpServer(80);

//-------------------------------------------------------------------------------------------
void setup(void)
{
    // Setup the One Wire Temperature bus
    ow_setPin(OW_BUS_PIN);

    Serial.begin(9600);
    while(!Serial) { }
}

void MQTTconnect(void)
{
    log("[MQTT] Connecting to Losant...");

    int retries = 0;
    while(!MQTTclient.isConnected() && retries < 5)
    {
        MQTTclient.connect(
            LOSANT_DEVICE_ID,
            LOSANT_ACCESS_KEY,
            LOSANT_ACCESS_SECRET);

        if(MQTTclient.isConnected())
        {
            log("[MQTT] Connected!");
            MQTTclient.subscribe(MQTT_TOPIC_COMMAND);
        }
        else
        {
            log("[MQTT] not connected, will try again");
            delay(5000);
            retries++;
        }
    }
}


void loop(void)
{
    float temp_readings[] = {0.0, 0.0};

    // --------------------------------
    // Temperature Reading Actions
    if (!read_temperature(temp_readings, 2))
    {
        // Failure
        log("[TEMP] read failure");
        temp_readings[0] = 99.99;
        temp_readings[1] = 99.99;
    }

    // --------------------------------
    // MQTT Actions
    if (!MQTTclient.isConnected())
    {
        MQTTconnect();
    }

    // Loop the MQTT client.
    MQTTclient.loop();

    int now = millis();

    // Publish state every 5 seconds.
    if(now - MQTTlastUpdate > 5000) {
        MQTTlastUpdate = now;

        // Build the json payload:
        // { "data" : { "tempF" : val, "tempC" : val }}
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
        JsonObject& state = jsonBuffer.createObject();

        // TODO: refer to your specific temperature sensor
        // on how to convert the raw voltage to a temperature.
        int tempRaw = analogRead(A0);
        state["tempLocal"] = temp_readings[0];
        state["tempProbe"] = temp_readings[1];
        root["data"] = state;

        // Get JSON string.
        char buffer[200];
        root.printTo(buffer, sizeof(buffer));

        MQTTclient.publish(MQTT_TOPIC_STATE, buffer);
    }

    delay(1000);

    // --------------------------------
    // SSDP Actions
    ssdpServer.processConnection();
}

void log(char* msg)
{
    if (msg == NULL)
        return;

    if (*msg == '\0')
        return;

    Particle.publish("log", msg);
    delay(500);
}

// Toggles the LED on/off whenever "toggle" command is received.
bool ledValue = false;
void callback(char* topic, byte* payload, unsigned int length) {

    // Parse the command payload.
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& command = jsonBuffer.parseObject((char*)payload);

    log("Command received:");
    command.printTo(logmsg, 64);
    log(logmsg);

    // If the command's name is "toggle", flip the LED.
    if(String(command["name"].asString()).equals(String("toggle"))) {
        ledValue = !ledValue;
        //digitalWrite(LED, ledValue ? HIGH : LOW);
        log("fake toggling LED");
    }
}

bool read_temperature(float data[], int dataLen)
{
    uint8_t subzero, cel, cel_frac_bits;
    uint8_t sensor_data_buffer[80];

    // Asks all DS18x20 devices to start temperature measurement, takes up to 750ms at max resolution
    DS18X20_start_meas( DS18X20_POWER_PARASITE, NULL );
    delay(1000);

    // Asks for the number of sensors
    uint8_t numsensors = ow_search_sensors(10, sensor_data_buffer);

    // loop through each sensor, and get the temperature data
    for (uint8_t i = 0; i < numsensors && i < dataLen; i++)
    {
        if (sensor_data_buffer[i * OW_ROMCODE_SIZE + 0] == DS18S20_ID || sensor_data_buffer[i * OW_ROMCODE_SIZE + 0] == DS18B20_ID) //0x10=DS18S20, 0x28=DS18B20
        {
            if (DS18X20_read_meas(&sensor_data_buffer[i*OW_ROMCODE_SIZE], &subzero, &cel, &cel_frac_bits) == DS18X20_OK)
            {
                float val = (float)cel + ((float)(cel_frac_bits * DS18X20_FRACCONV) / 10000.0) * (subzero ? -1.0 : 1.0);
                data[i] = val;
            }
            else
            {
                return false;
            }
        }
    }

    return true;
}
