#include <Adafruit_Fingerprint.h>

uint8_t verifyFingerprint(Adafruit_Fingerprint fingerprintSensor)
{
    uint8_t p = fingerprintSensor.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
    case FINGERPRINT_NOFINGER:
        Serial.println("No finger detected");
        return p;
    case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        return p;
    case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        return p;
    default:
        Serial.println("Unknown error");
        return p;
    }

    // OK success!

    p = fingerprintSensor.image2Tz();
    switch (p)
    {
    case FINGERPRINT_OK:
        Serial.println("Image converted");
        break;
    case FINGERPRINT_IMAGEMESS:
        Serial.println("Image too messy");
        return p;
    case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        return p;
    case FINGERPRINT_FEATUREFAIL:
        Serial.println("Could not find fingerprint features");
        return p;
    case FINGERPRINT_INVALIDIMAGE:
        Serial.println("Could not find fingerprint features");
        return p;
    default:
        Serial.println("Unknown error");
        return p;
    }

    // OK converted!
    p = fingerprintSensor.fingerSearch();
    if (p == FINGERPRINT_OK)
    {
        Serial.println("Found a print match!");
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error");
        return p;
    }
    else if (p == FINGERPRINT_NOTFOUND)
    {
        Serial.println("Did not find a match");
        return p;
    }
    else
    {
        Serial.println("Unknown error");
        return p;
    }

    // found a match!
    Serial.print("Found ID #");
    Serial.print(fingerprintSensor.fingerID);
    Serial.print(" with confidence of ");
    Serial.println(fingerprintSensor.confidence);

    return fingerprintSensor.fingerID;
}

// returns -1 if failed, otherwise returns ID #
int verifyAllFingerprint(Adafruit_Fingerprint fingerprintSensor)
{
    uint8_t p = fingerprintSensor.getImage();
    if (p != FINGERPRINT_OK)
        return -1;

    p = fingerprintSensor.image2Tz();
    if (p != FINGERPRINT_OK)
        return -1;

    p = fingerprintSensor.fingerFastSearch();
    if (p != FINGERPRINT_OK)
        return -1;

    // found a match!
    Serial.print("Found ID #");
    Serial.print(fingerprintSensor.fingerID);
    Serial.print(" with confidence of ");
    Serial.println(fingerprintSensor.confidence);
    return fingerprintSensor.fingerID;
}