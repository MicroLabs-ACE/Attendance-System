#include <Adafruit_Fingerprint.h>
#include "enrolFingerprint.h"
#include "verifyFingerprint.h"

const int TX = 4;
const int RX = 5;

// Operational Codes
const char ENROL = 'E';
const char ENROL_ALL = 'F';

const char VERIFY = 'V';
const char VERIFY_ALL = 'W';

const char DELETE = 'D';
const char DELETE_ALL = 'C';

char op_code;
bool isEnteredOp = false;

uint8_t enrolID = 0;
uint8_t verifyID;

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
      enrolID = Serial.parseInt();
      if (enrolID == 0)
      {
        return;
      }
      enrolFingerprint(fingerprintSensor, enrolID);
      break;

    case ENROL_ALL:
      break;

    // Verify
    case VERIFY:
      if (fingerprintSensor.templateCount == 0)
      {
        Serial.println("Sensor doesn't contain any fingerprint data. Please run the 'enrolFingerprint'.");
      }
      else
      {
        Serial.println("Waiting for valid finger...");
        Serial.print("Sensor contains ");
        Serial.print(fingerprintSensor.templateCount);
        Serial.println(" templates");
      }
      verifyID = verifyFingerprint(fingerprintSensor);
      break;

    case VERIFY_ALL:
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
