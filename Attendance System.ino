/***************************************************
This is the base library for the fingerprint sensor.
 Designed specifically to work with the Adafruit Fingerprint sensor
 ----> http://www.adafruit.com/products/751
These displays use TTL Serial to communicate, 2 pins are required to interface
 ****************************************************/

#include <Adafruit_Fingerprint.h>
#include "enrollFingerprint.h"
#include "verifyFingerprint.h"
#include "deleteFingerprint.h"

// serial ports
//  pin TX is IN from sensor (GREEN wire)
//  pin #RX is OUT from arduino  (WHITE wire)
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

// used to hold the entered operation code
char op_code;

// true if op_code != null.
bool isEnteredOp = false;

uint8_t enrolId = 0;
uint8_t deleteId = 0;
uint8_t verifyId;

// true if operation is stopped.
bool isStopping;

// For UNO and others without hardware serial, we must use software serial.
// Set up the serial port to use softwareserial..
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
      enroll();
      break;

    // Enroll All
    case ENROL_ALL:
      enrollAll();
      break;

    // Verify
    case VERIFY:
      verify();
      break;
    
    // Verify All
    case VERIFY_ALL:
      verifyAll();
      break;

    // Delete
    case DELETE:
      deletePrint();
      break;

    // Delete All
    case DELETE_ALL:
      deleteAll();
      break;
    }

    isEnteredOp = false;
    Serial.println();
  }

  delay(100);
}

void enroll()
{
  Serial.println("Ready to enroll a fingerprint!");
  Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");
  while (!Serial.available())
    ;
  enrolId = Serial.parseInt();
  if (enrolId == 0)
  {
    return;
  }
  enrollFingerprint(fingerprintSensor, enrolId);
  Serial.print("Fingerprint has ");
  Serial.print(fingerprintSensor.getTemplateCount());
  Serial.println(" fingerprints in memory.");
}

void enrollAll()
{
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
    enrolId = Serial.parseInt();
    if (enrolId == 0)
    {
      return;
    }

    int p = -1;
    p = fingerprintFirstImageCapture(fingerprintSensor, enrolId);

    if (p != FINGERPRINT_OK)
    {
      Serial.println("Error capturing first fingerprint");
      break;
    }

    isStopping = shouldStop();
    if (isStopping)
    {
      Serial.println("Burst mode ended!");
      break;
    }

    p = fingerprintSecondImageCapture(fingerprintSensor, enrolId);

    if (p != FINGERPRINT_OK)
    {
      Serial.println("Error capturing second fingerprint");
      break;
    }

    isStopping = shouldStop();
    if (isStopping)
    {
      Serial.println("Burst mode ended!");
      break;
    }

    p = fingerprintImageStorage(fingerprintSensor, enrolId);

    if (p != FINGERPRINT_OK)
    {
      break;
    }

    isStopping = shouldStop();
    if (isStopping)
    {
      Serial.println("Burst mode ended!");
      break;
    }
  }
}

void verify()
{
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
}

void verifyAll()
{
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

      if (verifyId == -1)
      {
        Serial.println("Unable to verify all fingerprints.");
      }

      isStopping = shouldStop();
      if (isStopping)
      {
        Serial.println("Burst mode ended!");
        break;
      }
    }
  }
}

void deletePrint()
{
  Serial.println("Please type in the ID # (from 1 to 127) you want to delete...");

  while (!Serial.available())
    ;
  deleteId = Serial.parseInt();
  if (deleteId == 0)
  {
    Serial.println("Please enter a valid ID.");
    return;
  }

  if (fingerprintSensor.getTemplateCount() == 0)
  {
    Serial.println("Sensor doesn't contain any fingerprint data. Please enrol fingerprints first.");
  }
  else
  {

    deleteFingerprint(fingerprintSensor, deleteId);
  }
}

void deleteAll()
{
  Serial.println("Fingerprint sensor is now operating in burst delete mode.");
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

      deleteAllFingerprint(fingerprintSensor);

      isStopping = shouldStop();
      if (isStopping)
      {
        Serial.println("Burst mode ended!");
        break;
      }
    }
  }
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
