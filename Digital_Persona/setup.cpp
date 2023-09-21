#include <conio.h>
#include <iostream>
#include <sqlite3.h>
#include <string>

#include "dpfj.h"
#include "dpfpdd.h"

using namespace std;

// Global variable to store the SQLite database connection
sqlite3 *db = nullptr;

int status;

DPFPDD_DEV deviceHandle = nullptr;

DPFPDD_CAPTURE_PARAM captureParam;
DPFPDD_CAPTURE_RESULT captureResult;

DPFPDD_IMAGE_FMT fingerprintImageFormat = DPFPDD_IMG_FMT_ISOIEC19794;
unsigned char *fingerprintImageData;
unsigned int fingerprintImageSize;

DPFJ_FMD_FORMAT fingerprintFMDFormat = DPFJ_FMD_ISO_19794_2_2005;
unsigned char *fingerprintFMData;
unsigned int fingerprintFMDataSize;

unsigned char **fingerprints;
unsigned int *fingerprintSizes;

// Function to handle DPFPDD and DPFJ errors
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

void setupFingerprintDevice()
{
    DPFPDD_DEV_INFO devInfoArray[2];
    unsigned int deviceCount = sizeof(devInfoArray) / sizeof(devInfoArray[0]);
    unsigned int deviceIndex;

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

void captureAndConvertFingerprint()
{
    captureParam = {0};
    captureParam.size = sizeof(captureParam);
    captureParam.image_fmt = DPFPDD_IMG_FMT_ISOIEC19794;
    captureParam.image_proc = DPFPDD_IMG_PROC_NONE;
    captureParam.image_res = 500;

    fingerprintImageSize = 500000;
    fingerprintImageData = (unsigned char *)malloc(fingerprintImageSize);

    captureResult = {0};
    captureResult.size = sizeof(captureResult);
    captureResult.info.size = sizeof(captureResult.info);

    // === Capture fingerprint ===
    status = dpfpdd_capture(deviceHandle, &captureParam, (unsigned int)(-1), &captureResult, &fingerprintImageSize, fingerprintImageData);
    if (status)
        handleError("Couldn't capture fingerprint.");

    // === Convert fingerprint ===
    fingerprintFMData = NULL;
    fingerprintFMDataSize = MAX_FMD_SIZE;

    status = dpfj_create_fmd_from_fid(fingerprintImageFormat, fingerprintImageData, fingerprintImageSize, fingerprintFMDFormat, fingerprintFMData, &fingerprintFMDataSize);
    if (status)
        handleError("Couldn't convert fingerprint to FMD.");

    cout << "Success: Captured and converted fingerprint image to FMD." << endl;
}

// Function to insert binary data (fingerprint) into the database
void enrollFingerprint()
{
    const char *insertSQL = "INSERT INTO fingerprintTable (binary_data, size) VALUES (?, ?);";

    // Prepare the SQL statement
    sqlite3_stmt *stmt;
    status = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, 0);
    if (status != SQLITE_OK)
        handleError("Could not enroll fingerprint.");

    sqlite3_bind_blob(stmt, 1, fingerprintFMData, fingerprintFMDataSize, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, fingerprintFMDataSize);

    status = sqlite3_step(stmt);
    if (!status)
        handleError("Could not enroll fingerprint.");

    sqlite3_finalize(stmt);

    cout << "Success: Enrolled fingerprint." << endl;
}

unsigned char **retrieveFingerprints(int &numFingerprints, unsigned int *&fingerprintSizes)
{
    unsigned char **fingerprints = nullptr;
    fingerprintSizes = nullptr;
    numFingerprints = 0;

    const char *selectSQL = "SELECT binary_data, size FROM fingerprintTable;";
    sqlite3_stmt *stmt;

    status = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, 0);
    if (status != SQLITE_OK)
    {
        cerr << "Error preparing SQL statement: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return fingerprints;
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
        sqlite3_close(db);
        delete[] fingerprints;
        delete[] fingerprintSizes;
        return fingerprints;
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

    return fingerprints;
}

void verifyFingerprint()
{
    int numFingerprints;
    unsigned int *fingerprintSizes;
    unsigned char **allFingerprints = retrieveFingerprints(numFingerprints, fingerprintSizes);

    unsigned int thresholdScore = 5;
    unsigned int candidateCnt;
    DPFJ_CANDIDATE candidate;

    status = dpfj_identify(fingerprintFMDFormat, fingerprintFMData, fingerprintFMDataSize, 0, fingerprintFMDFormat, numFingerprints, allFingerprints, fingerprintSizes, thresholdScore, &candidateCnt, &candidate);
    if (!status)
        handleError("Could not verify fingerprint.");

    cout << "Fingerprint Index: " << candidate.fmd_idx << endl;
}

int main()
{
    setupFingerprintDevice();
    initialiseDatabase();
    // for (unsigned int i = 0; i < 4; i++)
    // {
    //     captureAndConvertFingerprint();
    //     enrollFingerprint();
    // }

    captureAndConvertFingerprint();
    verifyFingerprint();

    dpfpdd_exit();
    _getch();
    return 0;
}
