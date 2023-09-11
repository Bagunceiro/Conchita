#include <conchita.h>
#include <WiFi.h>


const char* ssid = "ssid";
const char* psk = "secret";

Conchita conchita(1685);

void setup()
{
    Serial.begin(115400);

    WiFi.begin(ssid, psk);

    conchita.start();
}

void loop()
{

}