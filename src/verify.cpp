#include <conio.h>
#include <iostream>
#include <sqlite3.h>
#include <string>

#include "dpfj.h"
#include "dpfpdd.h"

using namespace std;

sqlite3 *db = nullptr;

DPFPDD_DEV deviceHandle;

int status;

void handleDPFPDDError(int errorCode)
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

void handleDPFJError(int errorCode) {}

void handleSQLiteError(int errorCode) {}

void initialiseDatabase()
{
    status = sqlite3_open("fingerprints.db", &db);
    if (status != SQLITE_OK)
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        _getch();
        exit(1);
    }

    const char *createTableSQL = "CREATE TABLE IF NOT EXISTS fingerprintTable (id INTEGER PRIMARY KEY AUTOINCREMENT, binary_data BLOB, size INTEGER);";
    status = sqlite3_exec(db, createTableSQL, 0, 0, 0);

    if (status != SQLITE_OK)
    {
        cerr << "Error creating table: " << sqlite3_errmsg(db) << endl;
        _getch();
        exit(1);
    }
}

void setupFingerprintDevice()
{
    status = dpfpdd_init();
    if (status != DPFPDD_SUCCESS)
    {
        cerr << "Error in initialising DPFPDD." << endl;
        _getch();
        exit(1);
    }

    DPFPDD_DEV_INFO deviceInfoArray[2];
    unsigned int deviceCount = sizeof(deviceInfoArray) / sizeof(deviceInfoArray[0]);
    status = dpfpdd_query_devices(&deviceCount, deviceInfoArray);
    if (status != DPFPDD_SUCCESS)
    {
        cerr << "Error in querying devices." << endl;
        _getch();
        exit(1);
    }

    if (deviceCount == 0)
    {
        cerr << "No devices found." << endl;
        _getch();
        exit(1);
    }

    unsigned int deviceIndex = 99;
    bool isExternalDevices = false;

    for (unsigned int i = 0; i < deviceCount; ++i)
    {
        string defaultDeviceStr = "&0000&0000";
        string selectDeviceStr = deviceInfoArray[i].name;
        if ((selectDeviceStr.find(defaultDeviceStr)))
        {
            // isExternalDevices = true;
            cout << i << ": " << deviceInfoArray[i].name << endl;
        }
    }

    // TODO: Check that device index entered is not the default device

    cout << "Enter device index: ";
    cin >> deviceIndex;

    if (deviceIndex == 99)
    {
        cerr << "No external fingerprint device." << endl;
        _getch();
        exit(1);
    }

    char *deviceName = deviceInfoArray[deviceIndex].name;

    status = dpfpdd_open_ext(deviceName, DPFPDD_PRIORITY_COOPERATIVE, &deviceHandle);
    if (status != DPFPDD_SUCCESS)
    {
        cerr << "Error in opening fingerprint device." << endl;
        _getch();
        exit(1);
    }
}

void retrieveFingerprints(int &numFingerprints, unsigned char **&fingerprints, unsigned int *&fingerprintSizes)
{
    fingerprints = nullptr;
    fingerprintSizes = nullptr;
    numFingerprints = 0;

    const char *selectSQL = "SELECT binary_data, size FROM fingerprintTable;";
    sqlite3_stmt *stmt;

    status = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, 0);
    if (status != SQLITE_OK)
    {
        cerr << "Error preparing SQL statement: " << sqlite3_errmsg(db) << endl;
        _getch();
        exit(1);
    }

    // Count the number of fingerprints in the database
    while (sqlite3_step(stmt) == SQLITE_ROW)
        numFingerprints++;

    // Allocate memory for the array of pointers and sizes
    fingerprints = new unsigned char *[numFingerprints];
    fingerprintSizes = new unsigned int[numFingerprints];

    // Reset the statement for data retrieval
    status = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, 0);
    if (status != SQLITE_OK)
    {
        cerr << "Error preparing SQL statement: " << sqlite3_errmsg(db) << endl;
        _getch();
        exit(1);
    }

    int fingerprintIndex = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const void *data = sqlite3_column_blob(stmt, 0);
        int dataSize = sqlite3_column_bytes(stmt, 0);
        int size = sqlite3_column_int(stmt, 1);

        if (data && dataSize > 0)
        {
            const unsigned char *fingerprintData = static_cast<const unsigned char *>(data); // Cast the pointer
            unsigned char *fingerprint = new unsigned char[dataSize];
            memcpy(fingerprint, fingerprintData, dataSize);
            fingerprints[fingerprintIndex] = fingerprint;
            fingerprintSizes[fingerprintIndex] = size;
            fingerprintIndex++;
        }
    }

    sqlite3_finalize(stmt);
}

void verifyFingerprint()
{
    cout << "Place finger..." << endl;

    DPFPDD_CAPTURE_PARAM captureParam = {0};
    captureParam.size = sizeof(captureParam);
    captureParam.image_fmt = DPFPDD_IMG_FMT_ISOIEC19794;
    captureParam.image_proc = DPFPDD_IMG_PROC_NONE;
    captureParam.image_res = 80;

    DPFPDD_CAPTURE_RESULT captureResult = {0};
    captureResult.size = sizeof(captureResult);
    captureResult.info.size = sizeof(captureResult.info);

    unsigned int fingerprintImageSize = 500000;
    unsigned char *fingerprintImageData = (unsigned char *)malloc(fingerprintImageSize);

    status = dpfpdd_capture(deviceHandle, &captureParam, (unsigned int)(-1), &captureResult, &fingerprintImageSize, fingerprintImageData);
    if (status != DPFPDD_SUCCESS)
    {
        cerr << "Could not capture fingerprint." << endl;
        _getch();
        exit(1);
    }

    DPFJ_FMD_FORMAT fingerprintFMDFormat = DPFJ_FMD_ISO_19794_2_2005;
    unsigned int fingerprintFMDSize = MAX_FMD_SIZE;
    unsigned char *fingerprintFMData = new unsigned char[fingerprintFMDSize];

    status = dpfj_create_fmd_from_fid(captureParam.image_fmt, fingerprintImageData, fingerprintImageSize, fingerprintFMDFormat, fingerprintFMData, &fingerprintFMDSize);
    status = dpfpdd_capture(deviceHandle, &captureParam, (unsigned int)(-1), &captureResult, &fingerprintImageSize, fingerprintImageData);
    if (status != DPFPDD_SUCCESS)
    {
        cerr << "Could not convert fingerprint." << endl;
        handleDPFPDDError(status);
        _getch();
        exit(1);
    }

    int numFingerprints;
    unsigned int *fingerprintSizes;
    unsigned char **fingerprints;

    retrieveFingerprints(numFingerprints, fingerprints, fingerprintSizes);

    unsigned int thresholdScore = 5;
    unsigned int candidateCount = 1;
    DPFJ_CANDIDATE candidate;

    status = dpfj_identify(fingerprintFMDFormat, fingerprintFMData, fingerprintFMDSize, 0, fingerprintFMDFormat, numFingerprints, fingerprints, fingerprintSizes, thresholdScore, &candidateCount, &candidate);
    if (status != DPFPDD_SUCCESS)
    {
        cerr << "Could not identify fingerprint." << endl;
        _getch();
        exit(1);
    }

    cout << "Fingerprint Index: " << candidate.fmd_idx << endl;
}

int main()
{
    initialiseDatabase();
    setupFingerprintDevice();
    verifyFingerprint();
    _getch();
    return 0;
}