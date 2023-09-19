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
    fingerprintDeviceSetup();
    initialiseDatabase();

    dpfpdd_exit();
    _getch();
    return 0;
}
