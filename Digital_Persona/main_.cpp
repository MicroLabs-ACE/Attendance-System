#include <conio.h>
#include <iostream>
#include <sqlite3.h>
#include <string>

#include "dpfj.h"
#include "dpfpdd.h"

using namespace std;

bool isError = false;

// Global variable to store the SQLite database connection
sqlite3 *db = nullptr;

DPFPDD_DEV_INFO devInfoArray[2];
unsigned int deviceCount = sizeof(devInfoArray) / sizeof(devInfoArray[0]);
DPFPDD_DEV deviceHandle = nullptr;
unsigned int deviceIndex = 0;
unsigned int dpi;

DPFPDD_CAPTURE_PARAM captureParam;
DPFPDD_CAPTURE_RESULT captureResult;

DPFPDD_IMAGE_FMT fingerprintImageFormat = DPFPDD_IMG_FMT_ISOIEC19794;
unsigned char *fingerprintImageData;
unsigned int fingerprintImageSize;

DPFJ_FMD_FORMAT fingerprintFMDFormat = DPFJ_FMD_ISO_19794_2_2005;
unsigned char *fingerprintFMData;
unsigned int fingerprintFMDataSize;

// Function to handle DPFPDD and DPFJ errors
void handleError(int errorCode)
{
    switch (errorCode)
    {
    case DPFPDD_E_FAILURE:
        cout << "DPFPDD_E_FAILURE: Unexpected failure." << endl;
        break;

    case DPFPDD_E_INVALID_DEVICE:
        cout << "DPFPDD_E_INVALID_DEVICE: Invalid reader handle." << endl;
        break;

    case DPFPDD_E_DEVICE_BUSY:
        cout << "DPFPDD_E_DEVICE_BUSY: Another operation is in progress." << endl;
        break;

    case DPFPDD_E_MORE_DATA:
        cout << "DPFPDD_E_MORE_DATA: Insufficient memory is allocated for the image_data. The required size is in the image_size." << endl;
        break;

    case DPFPDD_E_INVALID_PARAMETER:
        cout << "DPFPDD_E_INVALID_PARAMETER: Invalid parameter set in function." << endl;
        break;

    case DPFPDD_E_DEVICE_FAILURE:
        cout << "DPFPDD_E_DEVICE_FAILURE: Failed to start capture, reader is not functioning properly." << endl;
        break;

    default:
        cout << "Unknown error code: " << errorCode << endl;
        break;
    }
}

// Function to create an SQLite database and the fingerprintTable
bool initialiseDatabase()
{
    int rc = sqlite3_open("fingerprints.db", &db);

    if (rc)
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    // SQL statement to create the table with an auto-incremented primary key
    const char *createTableSQL = "CREATE TABLE IF NOT EXISTS fingerprintTable (id INTEGER PRIMARY KEY AUTOINCREMENT, binary_data BLOB, size INTEGER);";
    rc = sqlite3_exec(db, createTableSQL, 0, 0, 0);

    if (rc)
    {
        cerr << "Error creating table: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    return true;
}

void setup()
{
    int status;
    status = dpfpdd_init();
    if (!status)
        cerr << "DPFPDDInitError" << endl;

    initialiseDatabase();

    status = dpfpdd_query_devices(&deviceCount, devInfoArray);
    if (!status)
        cerr << "QueryDeviceError" << endl;

    if (deviceCount == 0)
        cerr << "NoDeviceError" << endl;

    string defaultDeviceName = "&0000&0000";
    if (deviceCount == 1)
    {
        string selectDeviceName = devInfoArray[0].name;
        size_t isDefaultDevice = selectDeviceName.find(defaultDeviceName);

        if (isDefaultDevice == string::npos)
            cerr << "DeviceNotFoundError" << endl;
    }
}

// Function to clean up resources and wait for a key press
void cleanup()
{
    // Close the SQLite database
    if (db)
    {
        int closeDBStatus = sqlite3_close(db);
        if (closeDBStatus != SQLITE_OK)
            cerr << "Error closing database: " << sqlite3_errmsg(db) << endl;

        else
            cout << "Closed database." << endl;
    }

    if (deviceHandle)
    {
        // Close DPFPDD device
        int closeDPFPDDStatus = dpfpdd_close(deviceHandle);

        if (closeDPFPDDStatus != DPFPDD_SUCCESS)
            handleError(closeDPFPDDStatus);
    }

    // Exit DPFPDD
    int exitDPFPDDStatus = dpfpdd_exit();
    if (exitDPFPDDStatus != DPFPDD_SUCCESS)
        handleError(exitDPFPDDStatus);

    // Wait for a key press
    cout << "Press any key to exit!" << endl;
    _getch();
}

bool captureAndConvertFingerprint()
{
    captureParam = {0};
    captureParam.size = sizeof(captureParam);
    captureParam.image_fmt = DPFPDD_IMG_FMT_ISOIEC19794;
    captureParam.image_proc = DPFPDD_IMG_PROC_NONE;
    captureParam.image_res = dpi;

    fingerprintImageSize = 500000;
    fingerprintImageData = (unsigned char *)malloc(fingerprintImageSize);

    captureResult = {0};
    captureResult.size = sizeof(captureResult);
    captureResult.info.size = sizeof(captureResult.info);

    // === Capture fingerprint ===
    int captureStatus = dpfpdd_capture(deviceHandle, &captureParam, (unsigned int)(-1), &captureResult, &fingerprintImageSize, fingerprintImageData);
    if (captureStatus != DPFPDD_SUCCESS)
    {
        handleError(captureStatus);
        return false;
    }
    cout << "Captured fingerprint image." << endl;

    // === Convert fingerprint ===
    fingerprintFMData = NULL;
    fingerprintFMDataSize = MAX_FMD_SIZE;

    int convertStatus = dpfj_create_fmd_from_fid(fingerprintImageFormat, fingerprintImageData, fingerprintImageSize, fingerprintFMDFormat, fingerprintFMData, &fingerprintFMDataSize);
    if (convertStatus != DPFPDD_SUCCESS)
    {
        handleError(convertStatus);
        return false;
    }
    cout << "Converted fingerprint image to FMD successfully." << endl;

    return true;
}

int main()
{
    // === Initialise DPFPDD ===
    int initDPFPDDStatus = dpfpdd_init();
    if (initDPFPDDStatus != DPFPDD_SUCCESS)
    {
        handleError(initDPFPDDStatus);
        cleanup();
        return 1;
    }

    cout << "Initialized DPFPDD." << endl;

    // === Initialise database and fingerprint table ===
    bool initDBStatus = initialiseDatabase();
    if (!initDBStatus)
    {
        cleanup();
        return 1;
    }

    // === Query devices ===
    DPFPDD_STATUS queryStatus;
    queryStatus = dpfpdd_query_devices(&deviceCount, devInfoArray);
    if (queryStatus != DPFPDD_SUCCESS)
    {
        handleError(queryStatus);
        cleanup();
        return 1;
    }
    cout << "Queried devices" << endl;

    if (deviceCount == 0)
    {
        cout << "No devices found." << endl;
        cleanup();
        return 1;
    }

    // === Get device parameters ===
    cout << "Found " << deviceCount << " device(s):" << endl;
    for (unsigned int i = 0; i < deviceCount; ++i)
        cout << "   Device " << i << ": " << devInfoArray[i].name << endl;

    if (deviceCount > 1)
    {
        cout << "Enter index of device: ";
        cin >> deviceIndex;
    }

    cout << "Enter dpi of device: ";
    cin >> dpi;

    char *deviceName = devInfoArray[deviceIndex].name;

    // === Open fingerprint device ===
    int openStatus = dpfpdd_open_ext(deviceName, DPFPDD_PRIORITY_COOPERATIVE, &deviceHandle);
    if (openStatus != DPFPDD_SUCCESS)
    {
        handleError(openStatus);
        cleanup();
        return 1;
    }
    cout << "Fingerprint device opened successfully." << endl;

    // === Capture and convert fingerprint ===
    bool captureAndConvertStatus = captureAndConvertFingerprint();
    if (!captureAndConvertStatus)
    {
        cleanup();
        return 1;
    }

    cleanup();
    return 0;
}
