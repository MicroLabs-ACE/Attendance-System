/*!
 * This is a library for verifying from an optical sensor
 *
 * Designed specifically to work with the Adafruit Fingerprint sensor
 * ----> http://www.adafruit.com/products/751
 *
 * These displays use TTL Serial to communicate, 2 pins are required to
 * interface
 *
 */

#include <Adafruit_Fingerprint.h>

/**************************************************************************/
/*!
    @brief verifies a fingerprint existence by getting the image and comparing it with the template in the db.
    @param fingerprintSensor  Fingerprint sensor from which the fingerprint would be deleted.
    @returns  Return the fingerprint id on success.
    @returns <code>FINGERPRINT_PACKETRECIEVEERR</code> Returned on communication error.
    @returns <code>FINGERPRINT_NOFINGER</code> Returned when no finger is detected.
    @returns <code>FINGERPRINT_IMAGEFAIL</code> Returned when imaging failed.
    @returns <code>FINGERPRINT_IMAGEMESS</code> Returned when image is messy.
    @returns <code>FINGERPRINT_FEATUREFAIL</code> Returned when fingerprint feature is not found.
    @returns <code>FINGERPRINT_INVALIDIMAGE</code> Returned when fingerprint feature is not found.
    @returns <code>FINGERPRINT_NOTFOUND</code> Returned when fingerprint is not found in the db.
*/
/**************************************************************************/
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

/**************************************************************************/
/*!
    @brief verifies all the fingerprints entered. It is a faster form of the method verifyFingerprint
    @param fingerprintSensor  Fingerprint sensor from which the fingerprint would be deleted.
    @returns  Return the fingerprint id on success.
    @returns -1 on failure to find fingerprint.
*/
/**************************************************************************/

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