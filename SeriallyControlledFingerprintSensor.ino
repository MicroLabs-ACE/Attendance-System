
// TODO: During setup, ensure fingerprint sensor has stored fingerprints in a stack format. If not, DeleteALL
// TODO: Check if fingerprint already exists in enroll, if so, send FingerprintAlreadyExists
// TODO: Ask for fingerprint ID before deleting
// led control

#include <Adafruit_Fingerprint.h>
#include <StopWatch.h>

// Timing
#define TIMEOUT 300000
#define DELAY_FOR_SECOND_CAPTURE 2000

// Constants
#define HAS_FINGERPRINT 0
#define HAS_NO_FINGERPRINT 12
#define FINGERPRINT_ADDRESS_SIZE 127
#define NEW_LINE_DELIMITER "\n"

// Commands
#define ENROLL "Enroll"
#define BURST_ENROLL "BurstEnroll"
#define VERIFY "Verify"
#define BURST_VERIFY "BurstVerify"
#define DELETE "Delete"
#define DELETE_ALL "DeleteAll"
#define STOP "Stop"

// Results
#define FINGERPRINT_SENSOR_SUCCESS "FingerprintSensorSuccess"
#define FINGERPRINT_SENSOR_ERROR "FingerprintSensorError"

#define FINGERPRINT_STORAGE_FULL "FingerprintStorageFull"
#define FINGERPRINT_STORAGE_EMPTY "FingerprintStorageEmpty"

#define FINGERPRINT_CONVERSION_ERROR "FingerprintConversionError"

#define FINGERPRINT_ENROLL_START "FingerprintEnrollStart"
#define FINGERPRINT_ENROLL_SUCCESS "FingerprintEnrollSuccess"
#define FINGERPRINT_ENROLL_MISMATCH "FingerprintEnrollMismatch"
#define FINGERPRINT_ENROLL_ERROR "FingerprintEnrollError"

#define FINGERPRINT_FIRST_CAPTURE "FingerprintFirstCapture"
#define FINGERPRINT_SECOND_CAPTURE "FingerprintSecondCapture"

#define FINGERPRINT_VERIFY_START "FingerprintVerifyStart"
#define FINGERPRINT_VERIFY_SUCCESS "FingerprintVerifySuccess"

#define FINGERPRINT_NOT_FOUND "FingerprintNotFound"
#define FINGERPRINT_ALREADY_EXIST "FingerprintAlreadyExists"

#define FINGERPRINT_DELETE_SUCCESS "FingerprintDeleteSuccess"
#define FINGERPRINT_DELETE_ALL_SUCCESS "FingerprintDeleteAllSuccess"

// Operation terminators
#define OPERATION_STOPPED "OperationStopped"
#define OPERATION_TIMEOUT "OperationTimeout"

const int TX = 2;
const int RX = 3;

SoftwareSerial sensorSerial(TX, RX);
Adafruit_Fingerprint fingerprintSensor = Adafruit_Fingerprint(&sensorSerial);
StopWatch stopWatch;

bool isAttendanceSystem = false;

bool isSensor;
uint8_t id;
String command;

uint8_t result;

bool isOperationEnd = false;

void setup()
{
  // Serial setup
  Serial.begin(9600);
  while (!Serial)
    ;
  delay(200);
  fingerprintSensor.begin(57600);

  // Check if fingerprint is connected
  isSensor = fingerprintSensor.verifyPassword();
  if (isSensor)
    println(FINGERPRINT_SENSOR_SUCCESS);
  else
    println(FINGERPRINT_SENSOR_ERROR);
}

void loop()
{
  if (isSensor)
  {
    if (Serial.available() > 0)
    {
      command = Serial.readStringUntil(NEW_LINE_DELIMITER);
      command.trim();
      resetTimeout();

      // Enroll
      if (command == ENROLL)
      {
        result = enrollFingerprint();
        println(result);
      }

      else if (command == BURST_ENROLL)
      {
        println(BURST_ENROLL);
        while (true)
        {

          result = enrollFingerprint();
           println(result);

          if (isOperationEnd)
            break;
        }
      }

      // Verify
      else if (command == VERIFY)
      {
        result = verifyFingerprint();
         println(result);
      }

      else if (command == BURST_VERIFY)
      {
        println(BURST_VERIFY);
        while (true)
        {
          result = verifyFingerprint();
           println(result);

          if (isOperationEnd)
            break;
        }
      }

      // Delete
      else if (command == DELETE)
      {
        result = deleteFingerprint(false);
         println(result);
      }

      else if (command == DELETE_ALL)
      {
        result = deleteFingerprint(true);
         println(result);
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
    stoppingCommand.trim();
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
String enrollFingerprint()
{
  id = 0;
  id = readId(true);
  if (!id)
    return FINGERPRINT_STORAGE_FULL;

  int p = -1; // Status checker

  println(FINGERPRINT_ENROLL_START);

  // Check whether to stop
  isOperationEnd = shouldStop();
  if (isOperationEnd)
    return OPERATION_STOPPED;

  resetTimeout();
  // First fingerprint image capture
  while (p != FINGERPRINT_OK)
  {
    p = fingerprintSensor.getImage();

    // Check whether to stop
    isOperationEnd = shouldStop();
    if (isOperationEnd)
      return OPERATION_STOPPED;

    isOperationEnd = shouldTimeout();
    if (isOperationEnd)
      return OPERATION_TIMEOUT;
  }

  // First fingerprint image capture
  println(FINGERPRINT_FIRST_CAPTURE);

  // First fingerprint image conversion
  p = fingerprintSensor.image2Tz(1);
  if (p != FINGERPRINT_OK)
    return FINGERPRINT_CONVERSION_ERROR;

  // Waiting for removal of finger
  delay(DELAY_FOR_SECOND_CAPTURE);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
    p = fingerprintSensor.getImage();
  p = -1;

  // Fingerprint search
  p = fingerprintSensor.fingerSearch();
  if (p == FINGERPRINT_OK)
    return FINGERPRINT_ALREADY_EXIST;

  resetTimeout();
  // Second fingerprint image capture
  while (p != FINGERPRINT_OK)
  {
    p = fingerprintSensor.getImage();

    // Check whether to stop
    isOperationEnd = shouldStop();
    if (isOperationEnd)
      return OPERATION_STOPPED;

    isOperationEnd = shouldTimeout();
    if (isOperationEnd)
      return OPERATION_TIMEOUT;
  }

  // Second fingerprint image capture
  println(FINGERPRINT_SECOND_CAPTURE);

  // Second fingerprint image conversion
  p = fingerprintSensor.image2Tz(2);
  if (p != FINGERPRINT_OK)
    return FINGERPRINT_CONVERSION_ERROR;

  // Fingerprint model creation
  p = fingerprintSensor.createModel();
  if (p != FINGERPRINT_OK)
  {
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
  println(FINGERPRINT_ENROLL_SUCCESS);
  return id;
}

String verifyFingerprint()
{
  int p = -1; // Status checker

  println(FINGERPRINT_VERIFY_START);

  id = 0;
  id = readId(false);
  if (!id)
    return FINGERPRINT_STORAGE_EMPTY;

  resetTimeout();
  // Fingerprint image capture
  while (p != FINGERPRINT_OK)
  {
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

    print(FINGERPRINT_VERIFY_SUCCESS);
    return id;
}

String deleteFingerprint(bool shouldDeleteAll)
{
  id = 0;
  id = readId(false);
  if (!id)
    return FINGERPRINT_STORAGE_EMPTY;

  if (shouldDeleteAll)
  {
    fingerprintSensor.emptyDatabase();
    println(FINGERPRINT_DELETE_ALL_SUCCESS);
    return id;
  }

  else
  {
    fingerprintSensor.deleteModel(id);
     println(FINGERPRINT_DELETE_SUCCESS);
      return id;
  }

  void println(uint8_t value)
  {
    if (isAttendanceSystem)
    {
      uint8_t output = -1;
      switch (value)
      {
      case FINGERPRINT_VERIFY_START:
      case FINGERPRINT_FIRST_CAPTURE:
      case FINGERPRINT_SECOND_CAPTURE:
      case FINGERPRINT_SENSOR_SUCCESS:
      case FINGERPRINT_ENROLL_START:
      case BURST_ENROLL:
      case BURST_VERIFY:
      case FINGERPRINT_ENROLL_SUCCESS:
      case FINGERPRINT_VERIFY_SUCCESS:
      case FINGERPRINT_DELETE_ALL_SUCCESS:
        output = -1;
        break;

      
      case FINGERPRINT_NOT_FOUND:
      case FINGERPRINT_SENSOR_ERROR:
      case FINGERPRINT_STORAGE_EMPTY:
      case FINGERPRINT_STORAGE_FULL:
      case OPERATION_STOPPED:
      case OPERATION_TIMEOUT:
      case FINGERPRINT_CONVERSION_ERROR:
      case FINGERPRINT_ALREADY_EXIST:
      case FINGERPRINT_CONVERSION_ERROR:
      case FINGERPRINT_ENROLL_MISMATCH:
      case FINGERPRINT_ENROLL_ERROR:
      case FINGERPRINT_ENROLL_MISMATCH:

        output = 0;
        break;

      default:
        output = value;
      }

      if (output != -1)
      {
        Serial.println(output);
      }
    }
    else
    {
      Serial.println(value);
    }
  }
}
