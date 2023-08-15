// TODO: Add FingerprintStorageEmpty check to verifyFingerprint()
// TODO: During setup, ensure fingerprint sensor has stored fingerprints in a stack format. If not, DeleteALL.


#include <Adafruit_Fingerprint.h>
#include <StopWatch.h>

#define TIMEOUT 500000
#define DELAY 2000
#define HAS_FINGERPRINT 0
#define HAS_NO_FINGERPRINT 12

#define FINGERPRINT_ADDRESS_SIZE 127

#define FINGERPRINT_SENSOR_SUCCESS "FingerprintSensorSuccess"
#define FINGERPRINT_SENSOR_ERROR "FingerprintSensorError"
#define FINGERPRINT_STORAGE_FULL "FingerprintStorageFull"
#define FINGERPRINT_FIRST_CAPTURE "FingerprintFirstCapture"
#define FINGERPRINT_SECOND_CAPTURE "FingerprintSecondCapture"
#define FINGERPRINT_CONVERSION_ERROR "FingerprintConversionError"
#define FINGERPRINT_ENROLL_MISMATCH "FingerprintEnrollMismatch"
#define FINGERPRINT_ENROLL_ERROR "FingerprintEnrollError"
#define FINGERPRINT_ENROLL_SUCCESS "FingerprintEnrollSuccess"
#define FINGERPRINT_NOT_FOUND "FingerprintNotFound"
#define FINGERPRINT_VERIFY_SUCCESS "FingerprintVerifySuccess"
#define FINGERPRINT_STORAGE_EMPTY "FingerprintStorageEmpty"
#define FINGERPRINT_DELETE_ALL_SUCCESS "FingerprintDeleteAllSuccess"
#define FINGERPRINT_DELETE_SUCCESS "FingerprintDeleteSuccess"

#define ENROLL "Enroll"
#define BURST_ENROLL "BurstEnroll"
#define VERIFY "Verify"
#define BURST_VERIFY "BurstVerify"
#define DELETE "Delete"
#define DELETE_ALL "DeleteAll"
#define STOP "Stop"

#define OPERATION_STOPPED "OperationStopped"
#define OPERATION_TIMEOUT "OperationTimeout"

#define NEW_LINE_DELIMITER "\n"

const int TX = 2;
const int RX = 3;

SoftwareSerial sensorSerial(TX, RX);
Adafruit_Fingerprint fingerprintSensor = Adafruit_Fingerprint(&sensorSerial);
StopWatch stopWatch;

bool isSensor;
uint8_t id;
String command;
String result;

bool isOperationEnd = false;

void setup() {
  // Serial setup
  Serial.begin(9600);
  while (!Serial)
    ;

  delay(DELAY);
  fingerprintSensor.begin(57600);

  // Check if fingerprint is connected
  isSensor = fingerprintSensor.verifyPassword();
  if (isSensor)
    Serial.println(FINGERPRINT_SENSOR_SUCCESS);
  else
    Serial.println(FINGERPRINT_SENSOR_ERROR);
}

void loop()
{
  if (isSensor)
  {
    if (Serial.available() > 0)
    {
      command = Serial.readStringUntil(NEW_LINE_DELIMITER);
      resetTimeout();

      // Enroll
      if (command == ENROLL)
      {
        result = enrollFingerprint();
        Serial.println(result);
      }

      else if (command == BURST_ENROLL)
      {
        Serial.println(BURST_ENROLL);
        while (true)
        {

          result = enrollFingerprint();
          Serial.println(result);

          if (isOperationEnd)
            break;
        }
      }

      // Verify
      else if (command == VERIFY)
      {
        result = verifyFingerprint();
        Serial.println(result);
      }

      else if (command == BURST_VERIFY)
      {
        Serial.println(BURST_VERIFY);
        while (true)
        {
          result = verifyFingerprint();
          Serial.println(result);

          if (isOperationEnd)
            break;
        }
      }

      // Delete
      else if (command == DELETE)
      {
        result = deleteFingerprint(false);
        Serial.println(result);
      }

      else if (command == DELETE_ALL)
      {
        result = deleteFingerprint(true);
        Serial.println(result);
      }
    }
  }
}

// Helper functions
bool shouldStop() {
  if (Serial.available() > 0) {
    String stoppingCommand = Serial.readStringUntil("\n");
    if (stoppingCommand == "Stop")
      return true;
  }
  return false;
}

bool shouldTimeout()
{
  if (!stopWatch.isRunning())
    stopWatch.start();

  if (stopWatch.elapsed() >= TIMEOUT)
    return true;

  return false;
}


void resetTimeout()
{
    stopWatch.reset();
}

void resetTimeout()
{
    stopWatch.reset();
}

int readId(bool isEnroll) {
  int id = 0;

  if (isEnroll) {
    for (int addr = 1; addr <= FINGERPRINT_ADDRESS_SIZE; addr++) {
      if (fingerprintSensor.loadModel(addr) == HAS_NO_FINGERPRINT) {
        id = addr;
        break;
      }
    }
  } else {
    for (int addr = FINGERPRINT_ADDRESS_SIZE; addr >= 1; addr--) {
      if (fingerprintSensor.loadModel(addr) == HAS_FINGERPRINT) {
        id = addr;
        break;
      }
    }
  }

  return id;
}

// Command operations
String enrollFingerprint() {
  id = 0;
  id = readId(true);
  if (!id)
    return FINGERPRINT_STORAGE_FULL;

  int p = -1;  // Status checker

  // Await first fingerprint image
  Serial.println(FINGERPRINT_FIRST_CAPTURE);

  // Check whether to stop
  isOperationEnd = shouldStop();
  if (isOperationEnd)
    return OPERATION_STOPPED;

  resetTimeout();
  // First fingerprint image capture
  while (p != FINGERPRINT_OK) {
    p = fingerprintSensor.getImage();

    // Check whether to stop
    isOperationEnd = shouldStop();
    if (isOperationEnd)
      return OPERATION_STOPPED;

    isOperationEnd = shouldTimeout();
    if (isOperationEnd)
      return OPERATION_TIMEOUT;
  }

  // First fingerprint image conversion
  p = fingerprintSensor.image2Tz(1);
  if (p != FINGERPRINT_OK)
    return FINGERPRINT_CONVERSION_ERROR;

  // Waiting for removal of finger
  delay(DELAY);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
    p = fingerprintSensor.getImage();
  p = -1;

  // Await second fingerprint image
  Serial.println(FINGERPRINT_SECOND_CAPTURE);

  resetTimeout();
  // Second fingerprint image capture
  while (p != FINGERPRINT_OK) {
    p = fingerprintSensor.getImage();

    // Check whether to stop
    isOperationEnd = shouldStop();
    if (isOperationEnd)
      return OPERATION_STOPPED;

    isOperationEnd = shouldTimeout();
    if (isOperationEnd)
      return OPERATION_TIMEOUT;
  }

  // Second fingerprint image conversion
  p = fingerprintSensor.image2Tz(2);
  if (p != FINGERPRINT_OK)
    return FINGERPRINT_CONVERSION_ERROR;

  // Fingerprint model creation
  p = fingerprintSensor.createModel();
  if (p != FINGERPRINT_OK) {
    if (p == FINGERPRINT_ENROLLMISMATCH)
      return FINGERPRINT_ENROLL_MISMATCH;

    else
      return FINGERPRINT_ENROLL_ERROR;
  }

  // Fingerprint model storage
  p = fingerprintSensor.storeModel(id);
  if (p != FINGERPRINT_OK)
    return FINGERPRINT_ENROLL_ERROR;

  // Fingerprint success
  return FINGERPRINT_ENROLL_SUCCESS;
}

String verifyFingerprint() {
  int p = -1;  // Status checker

  resetTimeout();
  // Fingerprint image capture
  while (p != FINGERPRINT_OK) {
    p = fingerprintSensor.getImage();

    // Check whether to stop
    isOperationEnd = shouldStop();
    if (isOperationEnd)
      return OPERATION_STOPPED;

    isOperationEnd = shouldTimeout();
    if (isOperationEnd)
      return OPERATION_TIMEOUT;
  }

  // Fingerprint image conversion
  p = fingerprintSensor.image2Tz();
  if (p != FINGERPRINT_OK)
    return FINGERPRINT_CONVERSION_ERROR;

  // Fingerprint search
  p = fingerprintSensor.fingerSearch();
  if (p != FINGERPRINT_OK)
    return FINGERPRINT_NOT_FOUND;

  return FINGERPRINT_VERIFY_SUCCESS;
}

String deleteFingerprint(bool shouldDeleteAll) {
  id = 0;
  id = readId(false);
  if (!id)
    return FINGERPRINT_STORAGE_EMPTY;

  if (shouldDeleteAll) {
    fingerprintSensor.emptyDatabase();
    return FINGERPRINT_DELETE_ALL_SUCCESS;
  }

  else {
    fingerprintSensor.deleteModel(id);
    return FINGERPRINT_DELETE_SUCCESS;
  }
}
