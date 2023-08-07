#include <Adafruit_Fingerprint.h>

int fingerprintImageCapture(Adafruit_Fingerprint fingerprintSensor);
int fingerprintImageConversion(int p);
int fingerprintFirstImageCapture(Adafruit_Fingerprint fingerprintSensor, uint8_t enrollId);
int fingerprintSecondImageCapture(Adafruit_Fingerprint fingerprintSensor, uint8_t enrollId);
int fingerprintImageStorage(Adafruit_Fingerprint fingerprintSensor, uint8_t enrollId);
uint8_t enrollFingerprint(Adafruit_Fingerprint fingerprintSensor, uint8_t enrollId);