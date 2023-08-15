import time
from enum import Enum

import serial


class SerialState(Enum):
    """The possible states of a game."""

    SETUP = 0
    WRITE = 1
    READ = 2


class SerialController:
    def __init__(self, port, baudrate) -> None:
        self.port = port
        self.baudrate = baudrate
        self.state = SerialState.SETUP

    def write_to_serial(self, data: str):
        """Writes data to the serial port.

        Args:
            port: The serial port name.
            data: The data to write.
        """
        with serial.Serial(self.port, baudrate=self.baudrate) as ser:
            ser.setDTR(False)
            time.sleep(1)
            ser.flushInput()
            ser.setDTR(True)
            time.sleep(2)
            ser.write(data.encode())

    def read_from_serial(self):
        """Reads data from the serial port.

        Args:
            port: The serial port name.

        Returns:
            The data read from the serial port.
        """
        with serial.Serial(self.port, baudrate=self.baudrate) as ser:
            ser.flushOutput()
            data = ser.readline().decode()
            return data

    def run(self):
        print("Running...")
        while True:
            if self.state == SerialState.SETUP:
                result = self.read_from_serial()
                print(f"Read: {result}")
                if "FingerprintSensorSuccess" in result:
                    self.state = SerialState.WRITE

            elif self.state == SerialState.WRITE:
                command = input("Command: ")
                print(f"Write: {command}")
                if command in ["Enroll", "Verify"]:
                    self.write_to_serial(f"{command}\n")
                    self.state = SerialState.READ

            elif self.state == SerialState.READ:
                result = self.read_from_serial()
                print(f"Read: {result}")
                self.state = SerialState.WRITE


if __name__ == "__main__":
    serial_controller = SerialController("COM12", 9600)
    serial_controller.run()
