/*!
 * This is a library for deleting from an optical sensor
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
    @brief  Delets a fingerprint based on the user id
    @param fingerprintSensor  Fingerprint sensor from which the fingerprint would be deleted.
    @param deleteId Id of the fingerprint to be deleted.
    @returns <code>FINGERPRINT_OK</code> Returned on success.
    @returns <code>FINGERPRINT_PACKETRECIEVEERR</code> Returned on communication error
    @returns <code>FINGERPRINT_BADLOCATION</code> Returned when the fingerprint could not be deleted from the sensor.
    @returns <code>FINGERPRINT_FLASHERR</code> Returned when there is an error writing to flash.
*/
/**************************************************************************/
uint8_t deleteFingerprint(Adafruit_Fingerprint fingerprintSensor, uint8_t deleteId)
{
    uint8_t p = -1;

    p = fingerprintSensor.deleteModel(deleteId);

    if (p == FINGERPRINT_OK)
    {
        Serial.println("Deleted!");
    }
    else if (p == FINGERPRINT_PACKETRECIEVEERR)
    {
        Serial.println("Communication error.");
    }
    else if (p == FINGERPRINT_BADLOCATION)
    {
        Serial.println("Could not delete in that location.");
    }
    else if (p == FINGERPRINT_FLASHERR)
    {
        Serial.println("Error writing to flash.");
    }
    else
    {
        Serial.print("Unknown error: 0x");
        Serial.println(p, HEX);
    }

    return p;
}

/**************************************************************************/
/*!
    @brief  Delets all fingerprints from the fingerprints sensor.
    @param fingerPrint  Fingerprint sensor from which the fingerprint would be deleted.
    @returns True if all fingerprints are deleted.
*/
/**************************************************************************/
boolean deleteAllFingerprint(Adafruit_Fingerprint fingerprintSensor)
{
    uint8_t p = -1;

    p = fingerprintSensor.emptyDatabase();

    if (p == FINGERPRINT_OK)
    {
        Serial.println("Deleted!");
        return true;
    }
    else
    {
        Serial.print("Uncaught error");
        return false;
    }
}