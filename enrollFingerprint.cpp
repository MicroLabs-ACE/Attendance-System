/*!
 * This is a library for enrolling into an optical sensor
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
    @brief getting a fingerprint image from the optical sensor. It keeps loopint till a fingerprint is found and the status is <code>FINGERPRINT_OK</code>
    @param fingerPrint  Fingerprint sensor from which the fingerprint would be deleted.
    @returns returns <code>FINGERPRINT_OK</code>  when the fingerprint has been found.

*/
/**************************************************************************/
int fingerprintImageCapture(Adafruit_Fingerprint fingerprintSensor)
{
    int p = -1;
    while (p != FINGERPRINT_OK)
    {
        p = fingerprintSensor.getImage();
        switch (p)
        {
        case FINGERPRINT_OK:
            Serial.println("Image taken.");
            return p;
        case FINGERPRINT_NOFINGER:

            break;
        case FINGERPRINT_PACKETRECIEVEERR:
            Serial.println("Communication error.");
            break;
        case FINGERPRINT_IMAGEFAIL:
            Serial.println("Imaging error.");
            break;
        default:
            Serial.println("Unknown error.");
            break;
        }
    }
}

/**************************************************************************/
/*!
    @brief prints out the current value to the serial output.
    @param p  value that is to be checked and printed to serial output.
    @returns <code>FINGERPRINT_OK</code> Returned on success.
    @returns <code>FINGERPRINT_IMAGEMESS</code> Returned when images are too messy.
    @returns <code>FINGERPRINT_PACKETRECIEVEERR</code> Returned on communication error.
    @returns <code>FINGERPRINT_FEATUREFAIL</code> Returned when fingerprint feature is not found.
    @returns <code>FINGERPRINT_INVALIDIMAGE</code> Returned when fingerprint feature is not found.

*/
/**************************************************************************/
int fingerprintImageConversion(int p)
{
    switch (p)
    {
    case FINGERPRINT_OK:
        Serial.println("Image converted");
        break;

    case FINGERPRINT_IMAGEMESS:
        Serial.println("Image too messy");
        break;
    case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
    case FINGERPRINT_FEATUREFAIL:
        Serial.println("Could not find fingerprint features");
        break;
    case FINGERPRINT_INVALIDIMAGE:
        Serial.println("Could not find fingerprint features");
        break;
    default:
        Serial.println("Unknown error");
        break;
    }
    return p;
}

/**************************************************************************/
/*!
    @brief  Captures the first fingerprint template.
    @param fingerprintSensor  Fingerprint sensor for capturing the fingerprint
    @param enrolId Unique id fot the template.
    @returns <code>FINGERPRINT_OK</code> Returned on success
    @returns <code>FINGERPRINT_IMAGEMESS</code> Returned if image is too messy
    @returns <code>FINGERPRINT_PACKETRECIEVEERR</code> Returned on communication error
    @returns <code>FINGERPRINT_FEATUREFAIL</code> Returned on failure to identify fingerprint features
    @returns <code>FINGERPRINT_INVALIDIMAGE</code> Returned on failure to identify fingerprint features
*/
/**************************************************************************/
int fingerprintFirstImageCapture(Adafruit_Fingerprint fingerprintSensor, uint8_t enrollId)
{
    Serial.print("Waiting for valid finger to enroll as #");
    Serial.println(enrollId);

    int p = fingerprintImageCapture(fingerprintSensor);
    p = fingerprintSensor.image2Tz(1);
    p = fingerprintImageConversion(p);

    return p;
}

/**************************************************************************/
/*!
    @brief  Captures the sencond fingerprint template.
    @param fingerprintSensor  Fingerprint sensor for capturing the fingerprint
    @param enrolId Unique id for the template.
    @returns <code>FINGERPRINT_OK</code> Returned on success
    @returns <code>FINGERPRINT_IMAGEMESS</code> Returned if image is too messy
    @returns <code>FINGERPRINT_PACKETRECIEVEERR</code> Returned on communication error
    @returns <code>FINGERPRINT_FEATUREFAIL</code> Returned on failure to identify fingerprint features
    @returns <code>FINGERPRINT_INVALIDIMAGE</code> Returned on failure to identify fingerprint features
*/
/**************************************************************************/
int fingerprintSecondImageCapture(Adafruit_Fingerprint fingerprintSensor, uint8_t enrolId)
{
    Serial.println("Remove finger");
    delay(2000);
    Serial.print("ID ");
    Serial.println(enrolId);

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

/**************************************************************************/
/*!
    @brief  Stores the model to the database.
    @param fingerprintSensor  Fingerprint sensor for capturing the fingerprint
    @param enrollId Unique id for the template.
    @returns <code>FINGERPRINT_OK</code> Returned on success
    @returns <code>FINGERPRINT_PACKETRECIEVEERR</code> Returned on communication error
    @returns <code>FINGERPRINT_ENROLLMISMATCH</code> Returned if captured prints don't match.
    @returns <code>FINGERPRINT_BADLOCATION</code> Returned if unable to save the print data.
    @returns <code>FINGERPRINT_FLASHERR</code> Returned if error occured while writing to the flash.
*/
/**************************************************************************/

int fingerprintImageStorage(Adafruit_Fingerprint fingerprintSensor, uint8_t enrolId)
{
    Serial.print("Creating model for #");
    Serial.println(enrolId);

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

    p = fingerprintSensor.storeModel(enrolId);
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

    return p;
}

/**************************************************************************/
/*!
    @brief  Stores the model to the database.
    @param fingerprintSensor  Fingerprint sensor for capturing the fingerprint
    @param enrollId Unique id for the template.

    @returns <code>FINGERPRINT_OK</code> Returned on success
    @returns <code>FINGERPRINT_IMAGEMESS</code> Returned if image is too messy
    @returns <code>FINGERPRINT_PACKETRECIEVEERR</code> Returned on communication error
    @returns <code>FINGERPRINT_FEATUREFAIL</code> Returned on failure to identify fingerprint features
    @returns <code>FINGERPRINT_INVALIDIMAGE</code> Returned on failure to identify fingerprint features
    @returns <code>FINGERPRINT_ENROLLMISMATCH</code> Returned if captured prints don't match.
    @returns <code>FINGERPRINT_BADLOCATION</code> Returned if unable to save the print data.
    @returns <code>FINGERPRINT_FLASHERR</code> Returned if error occured while writing to the flash.

*/
/**************************************************************************/
uint8_t enrollFingerprint(Adafruit_Fingerprint fingerprintSensor, uint8_t enrollId)
{
    int p = -1;
    p = fingerprintFirstImageCapture(fingerprintSensor, enrollId);
    if (p == FINGERPRINT_OK)
    {
        p = fingerprintSecondImageCapture(fingerprintSensor, enrollId);
        if (p == FINGERPRINT_OK)
        {
            p = fingerprintImageStorage(fingerprintSensor, enrollId);
        }
    }

    return p;
}