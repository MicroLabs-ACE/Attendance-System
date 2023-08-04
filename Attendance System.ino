#include <Adafruit_Fingerprint.h>
#include "enroll.h"

const int TX = 4;
const int RX = 5;

uint8_t id = 0;

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
  Serial.println("Ready to enroll a fingerprint!");
  Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");

  while (!Serial.available())
    ;
  id = Serial.parseInt();

  if (id == 0)
  {
    return;
  }

  while (!getFingerprintEnroll(fingerprintSensor, id))
    ;
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
