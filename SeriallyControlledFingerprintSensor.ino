// TODO: Delete by ID

#include <Adafruit_Fingerprint.h>
#include <StopWatch.h>

// Timing
#define TIMEOUT 300000
// DEBUG
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
#define EXTRACT "Extract"

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
#define FINGERPRINT_ALREADY_EXISTS "FingerprintAlreadyExists"

#define FINGERPRINT_DELETE_START "FingerprintDeleteStart"
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

bool isSensor;
uint8_t id;
String command;
String result;

bool isOperationEnd = false;

bool isAttendanceSystem = true;

void setup() {
  // Serial setup
  Serial.begin(9600);
  while (!Serial)
    ;
  delay(200);
  fingerprintSensor.begin(57600);

  Serial.println();

  // Check if fingerprint is connected
  isSensor = fingerprintSensor.verifyPassword();
  if (isSensor) {
    if (!isAttendanceSystem)
      Serial.println(FINGERPRINT_SENSOR_SUCCESS);
  }

  else {
    if (!isAttendanceSystem)
      Serial.println(FINGERPRINT_SENSOR_ERROR);
  }

  // LED off
  fingerprintSensor.LEDcontrol(false);
}

void loop() {
  if (isSensor) {
    if (Serial.available() > 0) {
      command = Serial.readStringUntil(NEW_LINE_DELIMITER);
      command.trim();
      resetTimeout();

      // Enroll
      if (command == ENROLL) {
        result = enrollFingerprint();
        serialPrinter(result);

        // LED off
        fingerprintSensor.LEDcontrol(false);
      }

      else if (command == BURST_ENROLL) {
        if (!isAttendanceSystem)
          Serial.println(BURST_ENROLL);

        while (true) {
          result = enrollFingerprint();
          serialPrinter(result);

          // LED off
          fingerprintSensor.LEDcontrol(false);
          delay(1500);

          if (isOperationEnd)
            break;
        }
      }

      // Verify
      else if (command == VERIFY) {
        result = verifyFingerprint();
        serialPrinter(result);

        // LED off
        fingerprintSensor.LEDcontrol(false);
      }

      else if (command == BURST_VERIFY) {
        if (!isAttendanceSystem)
          Serial.println(BURST_VERIFY);

        while (true) {
          result = verifyFingerprint();
          serialPrinter(result);

          // LED off
          fingerprintSensor.LEDcontrol(false);

          if (isOperationEnd)
            break;
        }
      }

      // Delete
      else if (command == DELETE) {
        result = deleteFingerprint(false);
        serialPrinter(result);

        // LED off
        fingerprintSensor.LEDcontrol(false);
      }

      else if (command == DELETE_ALL) {
        result = deleteFingerprint(true);
        serialPrinter(result);
      }

      // Extract
      else if (command == EXTRACT) {
        getModels();
        fingerprintSensor.LEDcontrol(false);
      }
    }
  }
}

// Helper functions
bool shouldStop() {
  if (Serial.available() > 0) {
    String stoppingCommand = Serial.readStringUntil("\n");
    stoppingCommand.trim();

    if (stoppingCommand == "Stop")
      return true;
  }

  return false;
}

bool shouldTimeout() {
  if (!stopWatch.isRunning())
    stopWatch.start();

  if (stopWatch.elapsed() >= TIMEOUT)
    return true;

  return false;
}

void resetTimeout() {
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
  }

  else {
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

  if (!isAttendanceSystem)
    Serial.println(FINGERPRINT_ENROLL_START);

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

  // First fingerprint image capture
  if (!isAttendanceSystem)
    Serial.println(FINGERPRINT_FIRST_CAPTURE);

  // First fingerprint image conversion
  p = fingerprintSensor.image2Tz(1);
  if (p != FINGERPRINT_OK)
    return FINGERPRINT_CONVERSION_ERROR;

  // Waiting for removal of finger
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
    p = fingerprintSensor.getImage();
  p = -1;

  // LED off
  fingerprintSensor.LEDcontrol(false);

  // Delay for second capture
  delay(DELAY_FOR_SECOND_CAPTURE);

  // Fingerprint search
  p = fingerprintSensor.fingerSearch();
  if (p == FINGERPRINT_OK)
    return FINGERPRINT_ALREADY_EXISTS;

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

  // Second fingerprint image capture
  if (!isAttendanceSystem)
    Serial.println(FINGERPRINT_SECOND_CAPTURE);

  // LED off
  fingerprintSensor.LEDcontrol(false);

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

  id = 0;
  id = readId(false);
  if (!id) {
    isOperationEnd = true;
    return FINGERPRINT_STORAGE_EMPTY;
  }

  id = 0;
  if (!isAttendanceSystem)
    Serial.println(FINGERPRINT_VERIFY_START);

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

  // LED off
  fingerprintSensor.LEDcontrol(false);

  delay(1000);

  // Fingerprint image conversion
  p = fingerprintSensor.image2Tz();
  if (p != FINGERPRINT_OK)
    return FINGERPRINT_CONVERSION_ERROR;

  // Fingerprint search
  p = fingerprintSensor.fingerSearch();
  if (p != FINGERPRINT_OK)
    return FINGERPRINT_NOT_FOUND;

  id = fingerprintSensor.fingerID;
  return FINGERPRINT_VERIFY_SUCCESS;
}

String deleteFingerprint(bool shouldDeleteAll) {
  int p = -1;  // Status checker

  if (!isAttendanceSystem)
    Serial.println(FINGERPRINT_DELETE_START);

  id = 0;
  id = readId(false);
  if (!id)
    return FINGERPRINT_STORAGE_EMPTY;

  if (shouldDeleteAll) {
    fingerprintSensor.emptyDatabase();
    id = 1;
    return FINGERPRINT_DELETE_ALL_SUCCESS;
  }

  else {
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

    // LED off
    fingerprintSensor.LEDcontrol(false);

    // Fingerprint image conversion
    p = fingerprintSensor.image2Tz();
    if (p != FINGERPRINT_OK)
      return FINGERPRINT_CONVERSION_ERROR;

    // Fingerprint search
    p = fingerprintSensor.fingerSearch();

    if (p == FINGERPRINT_OK)
      id = fingerprintSensor.fingerID;
    else
      return FINGERPRINT_NOT_FOUND;

    fingerprintSensor.deleteModel(id);
    return FINGERPRINT_DELETE_SUCCESS;
  }
}

void serialPrinter(String statusMessage) {
  if (isAttendanceSystem) {
    bool isSuccessMessage = false;

    String successArray[] = {
      // Enroll
      FINGERPRINT_ENROLL_SUCCESS,

      // Verify
      FINGERPRINT_VERIFY_SUCCESS,

      // Delete
      FINGERPRINT_DELETE_SUCCESS,
      FINGERPRINT_DELETE_ALL_SUCCESS,
    };

    for (int i = 0; i < sizeof(successArray); i++) {
      if (statusMessage == successArray[i]) {
        isSuccessMessage = true;
        Serial.println(id);
      }
    }

    if (!isSuccessMessage)
      Serial.println(0);
  }

  else {
    Serial.println(statusMessage);
  }
}

// FEATURE: Extract
void getModels() {
  Serial.println("[ Show Templete FULL ]");

  delay(2000);

  for (int finger = 1; finger <= 1; finger++) {
    downloadFingerprintTemplate(finger);
  }
}

uint8_t downloadFingerprintTemplate(uint16_t id) {
  Serial.print("Attempting to load #");
  Serial.println(id);
  int p = fingerprintSensor.loadModel(id);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.print("Template ");
      Serial.print(id);
      Serial.println(" loaded");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    default:
      Serial.print("Unknown error ");
      Serial.println(p);
      return p;
  }

  Serial.print("Attempting to get #");
  Serial.println(id);
  p = fingerprintSensor.getModel();  // FP_UPLOAD = UPCHAR 0x08  -getModel() for Char Buffer 1 and getM odel2() for Char Buffer 2-
  switch (p) {
    case FINGERPRINT_OK:
      Serial.print("Template ");
      Serial.print(id);
      Serial.println(" transferring:");
      break;
    default:
      Serial.print("Unknown error ");
      Serial.println(p);
      return p;
  }

  uint8_t bytesReceived[900];

  for (int i = 0; i < 900; i++) {
    bytesReceived[i] = 0;
  }

  int i = 0;
  while (i <= 554) {
    if (sensorSerial.available()) {


      bytesReceived[i++] = sensorSerial.read();
    }
  }

  Serial.println("Decoding packet...");

  // Filtering The Packet
  int a = 0, x = 3;
  Serial.print("uint8_t packet2[] = {");
  for (int i = 10; i <= 554; ++i) {
    a++;
    if (a >= 129) {
      i += 10;
      a = 0;
      Serial.println("};");
      Serial.print("uint8_t packet");
      Serial.print(x);
      Serial.print("[] = {");
      x++;
    } else {
      Serial.print("0x");
      printHex(bytesReceived[i - 1], 2);
      // Serial.print( bytesReceived[i-1]);

      Serial.print(", ");
      //Serial.print("/");
    }
  }
  Serial.println("};");
  Serial.println("COMPLETED\n");
}

void printHex(int num, int precision) {
  char tmp[16];
  char format[128];

  sprintf(format, "%%.%dX", precision);
  sprintf(tmp, format, num);
  Serial.print(tmp);
}
