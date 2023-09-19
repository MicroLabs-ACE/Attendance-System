#include <conio.h>
#include <iostream>
#include <sqlite3.h>
#include <string>

#include "dpfj.h"
#include "dpfpdd.h"

using namespace std;

// Global variable to store the SQLite database connection
sqlite3 *db = nullptr;

DPFPDD_DEV_INFO devInfoArray[2];
unsigned int deviceCount = sizeof(devInfoArray) / sizeof(devInfoArray[0]);
string deviceName;
DPFPDD_DEV deviceHandle = nullptr;
unsigned int deviceIndex = 0;
unsigned int dpi;

void handleError(const string &errorMsg)
{
    cerr << "Error: " << errorMsg << endl;
    dpfpdd_exit();
    _getch();
    exit(1);
}

void setup()
{
    int status;

    status = dpfpdd_init();
    if (status != DPFPDD_SUCCESS)
        handleError("DPFPDD initialization failed");

    status = dpfpdd_query_devices(&deviceCount, devInfoArray);
    if (status != DPFPDD_SUCCESS)
        handleError("Device query failed");

    string defaultDeviceName = "&0000&0010";
    size_t isDefaultDevice;

    if (deviceCount == 0)
        handleError("No fingerprint devices found");

    else
    {
        for (unsigned int i = 0; i < deviceCount; i++)
        {
            string selectDeviceName = devInfoArray[0].name;
            isDefaultDevice = selectDeviceName.find(defaultDeviceName);
            if (isDefaultDevice == string::npos)
                deviceName = selectDeviceName;
        }
    }

    if (deviceName.length() == 0)
        handleError("No external fingerprint devices found.");

    cout << "Success: Found fingerprint device." << endl;
}

int main()
{
    setup();

    dpfpdd_exit();
    _getch();
    return 0;
}
