#include <Adafruit_Fingerprint.h>

int fingerprintImageCapture(Adafruit_Fingerprint fingerprintSensor);
int fingerprintImageConversion(int p);
int fingerprintFirstImageCapture(Adafruit_Fingerprint fingerprintSensor, uint8_t enrolID);
int fingerprintSecondImageCapture(Adafruit_Fingerprint fingerprintSensor, uint8_t enrolID);
int fingerprintImageStorage(Adafruit_Fingerprint fingerprintSensor, uint8_t enrolID);
uint8_t enrolFingerprint(Adafruit_Fingerprint fingerprintSensor, uint8_t enrolID);