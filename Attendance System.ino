// TODO: Implement delete operations

#include <Adafruit_Fingerprint.h>

#define FINGERPRINT_ADDRESS_SIZE 127
#define HAS_FINGERPRINT 0
#define HAS_NO_FINGERPRINT 12

const int TX = 2;
const int RX = 3;

SoftwareSerial sensorSerial(TX, RX);
Adafruit_Fingerprint fingerprintSensor = Adafruit_Fingerprint(&sensorSerial);

bool isSensor;
uint8_t id;
String command;
String result;

void setup()
{
  // Serial setup
  Serial.begin(9600);
  while (!Serial)
    ;
  delay(2000);
  fingerprintSensor.begin(57600);

  // Check if fingerprint is connected
  isSensor = fingerprintSensor.verifyPassword();
  if (isSensor)
    Serial.println("FingerprintSensorSuccess");
  else
    Serial.println("FingerprintSensorError");
}

void loop()
{
  if (isSensor)
  {
    if (Serial.available() > 0)
    {
      command = Serial.readStringUntil("\n");

      // Enroll
      if (command == "Enroll")
      {
        result = fingerprintEnroll();
        Serial.println(result);
      }

      else if (command == "BurstEnroll")
      {
        while (true)
        {
          result = fingerprintEnroll();
          Serial.println(result);

          if (result == "OperationStopped")
            break;
        }
      }

      // Verify
      else if (command == "Verify")
      {
        result = fingerprintVerify();
        Serial.println(result);
      }

      else if (command == "BurstVerify")
      {
        while (true)
        {
          result = fingerprintVerify();
          Serial.println(result);

          if (result == "OperationStopped")
            break;
        }
      }

      // Delete
      else if (command == "Delete")
      {
        result = fingerprintDelete(false);
        Serial.println(result);
      }

      else if (command == "DeleteAll")
      {
        result = fingerprintDelete(true);
        Serial.println(result);
      }
    }
  }
}

// Helper functions
bool shouldStop()
{
  if (Serial.available() > 0)
  {
    String stoppingCommand = Serial.readStringUntil("\n");
    if (stoppingCommand == "Stop")
      return true;
  }
  return false;
}

int readId(bool isEnroll)
{
  int id = 0;

  if (isEnroll)
  {
    for (int addr = 1; addr <= FINGERPRINT_ADDRESS_SIZE; addr++)
    {
      if (fingerprintSensor.loadModel(addr) == HAS_NO_FINGERPRINT)
      {
        id = addr;
        break;
      }
    }
  }
  else
  {
    for (int addr = FINGERPRINT_ADDRESS_SIZE; addr >= 1; addr--)
    {
      if (fingerprintSensor.loadModel(addr) == HAS_FINGERPRINT)
      {
        id = addr;
        break;
      }
    }
  }

  return id;
}

// Command operations
String fingerprintEnroll()
{
  id = 0;
  id = readId(true);
  if (!id)
    return "FingerprintStorageFull";

  int p = -1; // Status checker

  // Await first fingerprint image
  // DEBUG: Make into status message
  Serial.print("Waiting for valid finger at #");
  Serial.print(id);
  Serial.println("...");

  // Check whether to stop
  if (shouldStop())
    return "OperationStopped";

  // First fingerprint image capture
  while (p != FINGERPRINT_OK)
  {
    p = fingerprintSensor.getImage();

    // Check whether to stop
    if (shouldStop())
      return "OperationStopped";
  }

  // First fingerprint image conversion
  p = fingerprintSensor.image2Tz(1);
  if (p != FINGERPRINT_OK)
    return "FingerprintConversionError";

  // Await second fingerprint image
  // DEBUG: Make into status message
  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
    p = fingerprintSensor.getImage();

  p = -1;
  Serial.println("Place same finger again");

  // Second fingerprint image capture
  while (p != FINGERPRINT_OK)
  {
    p = fingerprintSensor.getImage();

    // Check whether to stop
    if (shouldStop())
      return "OperationStopped";
  }

  // Second fingerprint image conversion
  p = fingerprintSensor.image2Tz(2);
  if (p != FINGERPRINT_OK)
    return "FingerprintConversionError";

  // Fingerprint model creation
  p = fingerprintSensor.createModel();
  if (p != FINGERPRINT_OK)
  {
    if (p == FINGERPRINT_ENROLLMISMATCH)
    {
      return "FingerprintEnrollMismatchError";
    }
    else
    {
      return "FingerprintEnrollUnknownError";
    }
  }

  // Fingerprint model storage
  p = fingerprintSensor.storeModel(id);
  if (p != FINGERPRINT_OK)
    return "FingerprintConversionError";

  // Fingerprint success
  return "FingerprintEnrollSuccess";
}

String fingerprintVerify()
{
  int p = -1; // Status checker

  // Fingerprint image capture
  while (p != FINGERPRINT_OK)
  {
    p = fingerprintSensor.getImage();

    // Check whether to stop
    if (shouldStop())
      return "OperationStopped";
  }

  // Fingerprint image conversion
  p = fingerprintSensor.image2Tz();
  if (p != FINGERPRINT_OK)
    return "FingerprintConversionError";

  // Fingerprint search
  p = fingerprintSensor.fingerSearch();
  if (p != FINGERPRINT_OK)
    return "FingerprintVerifyError";

  return "FingerprintVerifySuccess";
}

String fingerprintDelete(bool shouldDeleteAll)
{
  id = 0;
  id = readId(false);
  if (!id)
    return "FingerprintStorageEmpty";

  if (shouldDeleteAll)
  {
    fingerprintSensor.emptyDatabase();
    return "FingerprintDeleteAllSuccess";
  }

  else
  {
    fingerprintSensor.deleteModel(id);

    return "FingerprintDeleteSuccess";
  }
}
