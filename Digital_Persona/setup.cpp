#include <conio.h>
#include <iostream>
#include <sqlite3.h>
#include <string>

#include "dpfj.h"
#include "dpfpdd.h"

using namespace std;

// Global variable to store the SQLite database connection
sqlite3 *db = nullptr;

DPFPDD_DEV deviceHandle = nullptr;
unsigned int dpi;

void handleError(const string &errorMsg)
{
    cerr << "Error: " << errorMsg << endl;
    dpfpdd_exit();
    _getch();
    exit(1);
}

void initialiseDatabase()
{
    int rc = sqlite3_open("fingerprints.db", &db);

    if (rc)
        handleError("Can't open database.");

    // SQL statement to create the table with an auto-incremented primary key
    const char *createTableSQL = "CREATE TABLE IF NOT EXISTS fingerprintTable (id INTEGER PRIMARY KEY AUTOINCREMENT, binary_data BLOB, size INTEGER);";
    rc = sqlite3_exec(db, createTableSQL, 0, 0, 0);

    if (rc)
        handleError("Couldn't create fingerprints table.");

    cout << "Success: Initialised fingerprints database." << endl;
}

void fingerprintDeviceSetup()
{
    DPFPDD_DEV_INFO devInfoArray[2];
    unsigned int deviceCount = sizeof(devInfoArray) / sizeof(devInfoArray[0]);
    unsigned int deviceIndex;

    int status;

    status = dpfpdd_init();
    if (status != DPFPDD_SUCCESS)
        handleError("DPFPDD initialization failed");

    status = dpfpdd_query_devices(&deviceCount, devInfoArray);
    if (status != DPFPDD_SUCCESS)
        handleError("Device query failed");

    if (deviceCount == 0)
        handleError("No fingerprint devices found");

    else
    {
        string defaultDeviceName = "&0000&0010";
        string selectDeviceName;
        size_t isDefaultDevice;

        for (unsigned int i = 0; i < deviceCount; i++)
        {
            selectDeviceName = devInfoArray[i].name;
            isDefaultDevice = selectDeviceName.find(defaultDeviceName);
            if (isDefaultDevice == string::npos)
            {
                deviceIndex = i;
                break;
            }
        }
    }

    if (deviceIndex)
        handleError("No external fingerprint devices found.");

    char *deviceName = devInfoArray[deviceIndex].name;

    status = dpfpdd_open_ext(deviceName, DPFPDD_PRIORITY_COOPERATIVE, &deviceHandle);
    if (status != DPFPDD_SUCCESS)
        handleError("Couldn't open fingerprint device.");

    cout << "Success: Fingerprint device opened." << endl;
}

int main()
{
    fingerprintDeviceSetup();
    initialiseDatabase();

    dpfpdd_exit();
    _getch();
    return 0;
}
