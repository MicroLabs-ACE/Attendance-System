// TODO: Implement verification

#include <Adafruit_Fingerprint.h>

#define FINGERPRINT_ADDRESS_SIZE 127
#define HAS_NO_FINGERPRINT 12

SoftwareSerial sensorSerial(2, 3);
Adafruit_Fingerprint fingerprintSensor = Adafruit_Fingerprint(&sensorSerial);

uint8_t id;
bool isSensor;
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
  {
    Serial.println("FingerprintSensorSuccess");
  }
  else
  {
    Serial.println("FingerprintSensorFailure");
  }
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
        id = readId();
        if (!id)
        {
          Serial.println("FingerprintStorageNotFound");
        }
        else
        {
          Serial.println("FingerprintStorageFound");
          result = fingerprintEnroll();
          Serial.println(result);
        }
      }

      // Verify
      else if (command == "Verify")
      {
        result = fingerprintVerify();
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

int readId(void)
{
  int id = 0;
  for (int addr = 1; addr <= FINGERPRINT_ADDRESS_SIZE; addr++)
  {
    if (fingerprintSensor.loadModel(addr) == HAS_NO_FINGERPRINT)
    {
      id = addr;
      break;
    }
  }

  return id;
}

// Command operations
String fingerprintEnroll()
{
  int p = -1; // Status checker

  // Await first fingerprint image
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
      return "FingerprintEnrollMismatch";
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
  int p = -1;

  // Fingerprint image capture
  while (p != FINGERPRINT_OK)
  {
    p = fingerprintSensor.getImage();

    // Check whether to stop
    if (shouldStop())
      return "OperationStopped";
  }

  if (p != FINGERPRINT_OK)
    return "FingerprintCaptureError";

  p = fingerprintSensor.image2Tz();
  if (p != FINGERPRINT_OK)
    return "FingerprintConversionError";

  p = fingerprintSensor.fingerSearch();
  if (p != FINGERPRINT_OK)
    return "FingerprintVerifySuccess";

  return "FingerprintVerifyFailure";
}
