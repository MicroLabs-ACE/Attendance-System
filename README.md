# Attendance-System

This Arduino sketch provides functions to enroll, verify, and delete fingerprints using a fingerprint sensor module connected to an Arduino Uno. It is intended to work together with a Python program that sends commands over serial communication.

## Setup

- Connect the fingerprint sensor to the Arduino using the TX, RX, Voltage and GND pins.
- Note that by default TX is pin 2 and RX is pin 3.
- Upload the Arduino sketch to the board.
- The baudrate should be sent to 9600.

## Status messages

The Arduino will perform the operation and respond over serial with a message indicating

- Success - ending with `Success` indicates operation ended as it should.
- Error - ending with `Error` indicates operation didn't end as it should.
- Information

## Usage

Upon connection, the following messages can be sent:

- _FingerprintSensorSuccess_ - indicates there is a valid fingerprint sensor.
- _FingerprintSensorError_ - indicates there is no valid fingerprint sensor. No command can be carried out if this message is sent. Check connections (i.e. TX, RX, Voltage and GND pins) for fingerprint sensor and reconnect. If it persists, consult hardware engineer.

The message is sent about two seconds after connection.

The Python program sends the following commands over serial. They should be sent with a newline delimiter:

- **Enroll** - Enroll a new fingerprint.
- **Verify** - Verify a fingerprint
- **BurstEnroll/Verify** - Perform operation multiple times until told to stop
- **Delete** - Delete a single fingerprint
- **DeleteAll** - Delete all fingerprints
- **Stop** - Stops a command's execution mid-operation

### Enroll/BurstEnroll

The enroll process requires the fingerprint to be scanned twice. Follow the prompts in the serial monitor.
This can be canceled mid-enrollment by sending `Stop` over serial.

The following messages can be sent:

- _FingerprintStorageFull_ - indicates the no more fingerprints can be stored. Check fingerprint sensor product name to know storage capacity. Check ID selection for more information. You can run the `Delete` or `DeleteAll` command to free up space.
- _OperationStopped_ - indicates that a 'Stop' command was sent.
- _OperationTimeout_ - indicates that an operation has timed out after 5000s of inactivity. Consult hardware engineer to change timeout value.
- _FingerprintFirstCapture_ - indicates an enrollee's finger is being captured the first time.
- _FingerprintSecondCapture_ - indicates an enrollee's finger is being captured the second time.
- _FingerprintEnrollStart_ - indicates an enroll process has started.
- _FingerprintConversionError_ - indicates fingerprint image could not be converted. If it persists, consult hardware engineer.
- _FingerprintEnrollMismatch_ - indicates fingerprints from first capture and second capture didn't match. Clean fingerprint sensor surface as well as finger of enrollee and try again. Ensure enrollee's finger is the same and is placed properly. If it persists, consult hardware engineer.
- _FingerprintEnrollError_ - indicates an error occurred during fingerprint model creation or storage. If it persists, consult hardware engineer.
- _BurstEnroll_ - indicates that burst enroll has begun.

### Verify/BurstVerify

Simply place an already enrolled finger on the sensor when prompted.
This can be canceled mid-enrollment by sending `Stop` over serial.

The following messages can be sent:

- _OperationStopped_ - indicates that a 'Stop' command was sent.
- _OperationTimeout_ - indicates that an operation has timed out after 3 minutes of inactivity. Consult hardware engineer to change timeout value.
- _FingerprintConversionError_ - indicates fingerprint image could not be converted. If it persists, consult hardware engineer.
- _FingerprintVerifyStart_ - indicates a verify process has started.
- _FingerprintNotFound_ - indicates that fingerprint is not registered on fingerprint sensor. If it persists, try enrolling again. If it still persists, consult hardware engineer.
- _FingerprintVerifySuccess_ - indicates fingerprint was found.
- _BurstVerify_ - indicates that burst verify has begun.

### Delete/DeleteAll

No fingerprint scan is needed. Deletes either a single fingerprint or all fingerprints depending on the command.

The following messages can be sent:

- _FingerprintStorageEmpty_ - indicates no fingerprints are in storage. Register fingerprints by running the `Enroll` or `BurstEnroll` command. If it still persists, consult hardware engineer.
- _FingerprintDelete(All)Success_ - indicates fingerprint(s) has/have been successfully deleted.

**ID selection** - The fingerprint storage works using a LIFO (i.e. stack) mechanism.
