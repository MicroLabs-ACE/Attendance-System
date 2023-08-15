#include <Adafruit_Fingerprint.h>

uint8_t uploadFingerprint(Adafruit_Fingerprint fingerprintSensor, uint16_t packet)
{
    uint8_t p = fingerprintSensor.uploadModel(packet);
    switch (p)
    {
    case FINGERPRINT_OK:
        Serial.println("Upload success");
        break;
    case FINGERPRINT_BADPACKET:
        Serial.println("Bad packet");
        return p;
    case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        return p;

    default:
        Serial.println("Unknown error");
        return p;
    }

    return p;
}
