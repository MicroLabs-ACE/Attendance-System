#include <Adafruit_Fingerprint.h>

int fingerprintImageCapture(Adafruit_Fingerprint fingerprintSensor)
{
    int p = -1;
    while (p != FINGERPRINT_OK)
    {
        p = fingerprintSensor.getImage();
        switch (p)
        {
        case FINGERPRINT_OK:
            Serial.println("Image taken");
            return p;
        case FINGERPRINT_NOFINGER:
            break;
        case FINGERPRINT_PACKETRECIEVEERR:
            Serial.println("Communication error");
            break;
        case FINGERPRINT_IMAGEFAIL:
            Serial.println("Imaging error");
            break;
        default:
            Serial.println("Unknown error");
            break;
        }
    }
}

int fingerprintImageConversion(int p)
{
    switch (p)
    {
    case FINGERPRINT_OK:
        Serial.println("Image converted");
        return p;
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
}

int fingerprintFirstImageCapture(Adafruit_Fingerprint fingerprintSensor, uint8_t enrollId)
{
    Serial.print("Waiting for valid finger to enroll as #");
    Serial.println(enrollId);

    int p = fingerprintImageCapture(fingerprintSensor);
    p = fingerprintSensor.image2Tz(1);
    p = fingerprintImageConversion(p);

    return p;
}

int fingerprintSecondImageCapture(Adafruit_Fingerprint fingerprintSensor, uint8_t enrollId)
{
    Serial.println("Remove finger");
    delay(2000);
    Serial.print("ID ");
    Serial.println(enrollId);

    int p = 0;
    while (p != FINGERPRINT_NOFINGER)
    {
        p = fingerprintSensor.getImage();
    }
    p = fingerprintImageCapture(fingerprintSensor);
    p = fingerprintSensor.image2Tz(2);
    p = fingerprintImageConversion(p);

    return p;
}

int fingerprintImageStorage(Adafruit_Fingerprint fingerprintSensor, uint8_t enrollId)
{
    Serial.print("Creating model for #");
    Serial.println(enrollId);

    int p = fingerprintSensor.createModel();
    if (p == FINGERPRINT_OK)
    {
        Serial.println("Prints matched!");
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error");
        return p;
    }
    else if (p == FINGERPRINT_ENROLLMISMATCH)
    {
        Serial.println("Fingerprints did not match");
        return p;
    }
    else
    {
        Serial.println("Unknown error");
        return p;
    }

    p = fingerprintSensor.storeModel(enrollId);
    if (p == FINGERPRINT_OK)
    {
        Serial.println("Stored!");
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error");
        return p;
    }
    else if (p == FINGERPRINT_BADLOCATION)
    {
        Serial.println("Could not store in that location");
        return p;
    }
    else if (p == FINGERPRINT_FLASHERR)
    {
        Serial.println("Error writing to flash");
        return p;
    }
    else
    {
        Serial.println("Unknown error");
        return p;
    }

    return true;
}

uint8_t enrollFingerprint(Adafruit_Fingerprint fingerprintSensor, uint8_t enrollId)
{
    int p = -1;
    p = fingerprintFirstImageCapture(fingerprintSensor, enrollId);
    p = fingerprintSecondImageCapture(fingerprintSensor, enrollId);
    p = fingerprintImageStorage(fingerprintSensor, enrollId);

    return p;
}