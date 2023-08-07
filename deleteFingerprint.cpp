#include <Adafruit_Fingerprint.h>

uint8_t deleteFingerprint(Adafruit_Fingerprint fingerprintSensor, uint8_t deleteId)
{
    uint8_t p = -1;

    p = finger.deleteModel(deleteId);

    if (p == FINGERPRINT_OK)
    {
        Serial.println("Deleted!");
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error");
    }
    else if (p == FINGERPRINT_BADLOCATION)
    {
        Serial.println("Could not delete in that location");
    }
    else if (p == FINGERPRINT_FLASHERR)
    {
        Serial.println("Error writing to flash");
    }
    else
    {
        Serial.print("Unknown error: 0x");
        Serial.println(p, HEX);
    }

    return p;
}

bool deleteAllFingerprint(Adafruit_Fingerprint fingerprintSensor)
{
    fingerprintSensor.emptyDatabase();
}