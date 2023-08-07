#include <Adafruit_Fingerprint.h>
#include "enrollFingerprint.h"
#include "verifyFingerprint.h"

const int TX = 4;
const int RX = 5;

// Operational Codes
const char ENROL = 'E';
const char ENROL_ALL = 'F';

const char VERIFY = 'V';
const char VERIFY_ALL = 'W';

const char STOP = 'S';

const char DELETE = 'D';
const char DELETE_ALL = 'C';

char op_code;
bool isEnteredOp = false;

uint8_t enrollId = 0;
uint8_t verifyId;

bool isStopping;

SoftwareSerial sensorSerial(TX, RX);
Adafruit_Fingerprint fingerprintSensor = Adafruit_Fingerprint(&sensorSerial);

void setup()
{
  Serial.begin(9600);
  fingerprintSensor.begin(57600);

  delay(100);
  Serial.println("\n\nAdafruit Fingerprint sensor enrollment");

  bool isSensor = pingFingerprintSensor();
  if (isSensor)
  {
    Serial.println("Fingerprint sensor found!");
  }
  else
  {
    Serial.println("Fingerprint sensor not found :(");
  }

  delay(2000);
}

void loop()
{
  if (!isEnteredOp)
  {
    Serial.print("Enter opcode: ");
    isEnteredOp = true;
  }

  while (Serial.available() > 0)
  {
    op_code = Serial.read();

    switch (op_code)
    {
    // Enrol
    case ENROL:
      Serial.println("Ready to enroll a fingerprint!");
      Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");
      while (!Serial.available())
        ;
      enrollId = Serial.parseInt();
      if (enrollId == 0)
      {
        return;
      }
      enrollFingerprint(fingerprintSensor, enrollId);
      Serial.print("Fingerprint has ");
      Serial.print(fingerprintSensor.getTemplateCount());
      Serial.println(" fingerprints in memory.");
      break;

    case ENROL_ALL:
      Serial.println("Fingerprint sensor is now operating in burst enrol mode.");
      Serial.print("Enter '");
      Serial.print(STOP);
      Serial.println("' to stop operation.");
      while (true)
      {
        Serial.println("Ready to enroll a fingerprint!");
        Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");
        while (!Serial.available())
          ;
        enrollId = Serial.parseInt();
        if (enrollId == 0)
        {
          return;
        }

        int p = -1;
        p = fingerprintFirstImageCapture(fingerprintSensor, enrollId);
        isStopping = shouldStop();
        if (isStopping)
        {
          Serial.println("Burst mode ended!");
          break;
        }

        p = fingerprintSecondImageCapture(fingerprintSensor, enrollId);
        isStopping = shouldStop();
        if (isStopping)
        {
          Serial.println("Burst mode ended!");
          break;
        }

        p = fingerprintImageStorage(fingerprintSensor, enrollId);
        isStopping = shouldStop();
        if (isStopping)
        {
          Serial.println("Burst mode ended!");
          break;
        }
      }
      break;

    // Verify
    case VERIFY:
      if (fingerprintSensor.getTemplateCount() == 0)
      {
        Serial.println("Sensor doesn't contain any fingerprint data. Please enrol fingerprints.");
      }
      else
      {
        Serial.println("Waiting for valid finger...");
        Serial.print("Sensor contains ");
        Serial.print(fingerprintSensor.getTemplateCount());
        Serial.println(" templates");
        verifyId = verifyFingerprint(fingerprintSensor);
      }
      break;

    case VERIFY_ALL:
      Serial.println("Fingerprint sensor is now operating in burst verify mode.");
      Serial.print("Enter '");
      Serial.print(STOP);
      Serial.println("' to stop operation.");

      if (fingerprintSensor.getTemplateCount() == 0)
      {
        Serial.println("Sensor doesn't contain any fingerprint data. Please enrol fingerprints.");
      }
      else
      {
        while (true)
        {
          Serial.print("Sensor contains ");
          Serial.print(fingerprintSensor.getTemplateCount());
          Serial.println(" templates");
          verifyId = verifyAllFingerprint(fingerprintSensor);
          isStopping = shouldStop();
          if (isStopping)
          {
            Serial.println("Burst mode ended!");
            break;
          }
        }
      }
      break;

    // Delete
    case DELETE:
      break;

    case DELETE_ALL:
      break;
    }

    isEnteredOp = false;
    Serial.println();
  }

  delay(100);
}

bool pingFingerprintSensor()
{
  if (fingerprintSensor.verifyPassword())
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool shouldStop()
{
  if (Serial.available() > 0)
  {
    char shouldStop = Serial.read();
    if (shouldStop == STOP)
    {
      return true;
    }
  }
  return false;
}
