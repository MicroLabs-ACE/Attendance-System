# Serially Controlled Fingerprint Sensor

This Arduino sketch provides functions to enroll, verify, and delete fingerprint(s) using a fingerprint sensor module connected to an Arduino Uno. It is intended to work together with a Python program that sends commands over serial communication.

## Setup

- Connect the fingerprint sensor to the Arduino using the TX, RX, Voltage and GND pins. Check for which fingerprint sensor you want to use.
- Note that by default TX is pin 2 and RX is pin 3.
- Upload the Arduino sketch to the board.
- The baud rate should be set to 9600.

## Status messages

The following types of messages are passed across:

- Success - ending with `Success` indicates the operation ended as it should.
- Error - ending with `Error` indicates the operation didn't end as it should.
- Any other message is simply giving information as to what is happening.

## Usage

Upon connection, the following messages can be sent:

- _FingerprintSensorSuccess_ - indicates there is a valid fingerprint sensor.
- _FingerprintSensorError_ - indicates there is no valid fingerprint sensor. No command can be carried out if this message is sent. Check connections (i.e. TX, RX, Voltage and GND pins) for the fingerprint sensor and reconnect. If it persists, consult the hardware engineer.

The message is sent about two seconds after the USB connection.

The Python program can send the following commands over the serial connection. They should be sent with a newline delimiter:

- **Enroll** - Enroll a new fingerprint.
- **Verify** - Verify a fingerprint.
- **BurstEnroll/Verify** - Perform operation multiple times until told to stop or timeout.
- **Delete** - Delete a single fingerprint.
- **DeleteAll** - Delete all fingerprints.
- **Stop** - Stops a command's execution mid-operation.
- **Ping** - Ensures that the fingerprint sensor is still connected.

### Enroll/BurstEnroll

The enrollment process requires the fingerprint to be scanned twice. Follow the prompts in the serial monitor.
This can be cancelled mid-enrollment by sending `Stop` over the serial connection.

The following messages can be sent:

- _FingerprintStorageFull_ - indicates that no more fingerprints can be stored. Check the fingerprint sensor's product name to know storage capacity. Check ID selection for more information. You can run the `Delete` or `DeleteAll` command to free up space.
- _OperationStopped_ - indicates that a `Stop` command was sent.
- _OperationTimeout_ - indicates that an operation has timed out after 5 minutes of inactivity. Consult the hardware engineer to change the timeout value.
- _FingerprintFirstCapture_ - indicates an enrollee's finger has been captured the first time.
- _FingerprintSecondCapture_ - indicates an enrollee's finger has been captured the second time.
- _FingerprintEnrollStart_ - indicates an enrollment process has started.
- _FingerprintConversionError_ - indicates fingerprint image could not be converted. If it persists, consult the hardware engineer.
- _FingerprintEnrollMismatch_ - indicates fingerprints from the first capture and second capture didn't match. Clean the fingerprint sensor surface as well as the finger of the enrollee and try again. Ensure the enrollee's finger is the same and is placed properly. If it persists, consult the hardware engineer.
- _FingerprintEnrollError_ - indicates an error occurred during fingerprint model creation or storage. If it persists, consult the hardware engineer.
- _BurstEnroll_ - indicates that burst enroll has begun.

### Verify/BurstVerify

Simply place an already enrolled finger on the sensor when prompted.
This can be cancelled mid-enrollment by sending `Stop` over the serial connection.

The following messages can be sent:

- _OperationStopped_ - indicates that a `Stop` command was sent.
- _OperationTimeout_ - indicates that an operation has timed out after 5 minutes of inactivity. Consult the hardware engineer to change the timeout value.
- _FingerprintConversionError_ - indicates fingerprint image could not be converted. If it persists, consult the hardware engineer.
- _FingerprintVerifyStart_ - indicates a verification process has started.
- _FingerprintNotFound_ - indicates that the fingerprint is not registered on the fingerprint sensor. If it persists, try enrolling again. If it still persists, consult the hardware engineer.
- _FingerprintVerifySuccess_ - indicates fingerprint was found.
- _BurstVerify_ - indicates that burst verify has begun.

For `Burst` commands, it should be noted that there is a delay of 1500ms between successive operations. To modify, consult the hardware engineer.

### Delete/DeleteAll

The deletion process (activated by the `Delete` command) requires that your fingerprint be scanned to see if there is a match for the fingerprint in the database. If so, it deletes it. To empty the database, send `DeleteAll`.

The following messages can be sent:

- _FingerprintStorageEmpty_ - indicates no fingerprints are in storage. Register fingerprints by running the `Enroll` or `BurstEnroll` command. If it still persists, consult the hardware engineer.
- _FingerprintDelete(All)Success_ - indicates fingerprint(s) has/have been successfully deleted.

### Ping

The following message is sent:

- _PingSuccess_ - indicates a `Ping` command has been received and that the fingerprint sensor is still conencted.
- PingError - indicates a `Ping` command has been received and that the fingerprint sensor is not still conencted.
