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
    if (sqlite3_prepare_v2(db, insertSQL, -1, &stmt, 0) == SQLITE_OK)
    {
        // Bind the unsigned char array as a BLOB
        sqlite3_bind_blob(stmt, 1, fingerprintFMData, fingerprintFMDataSize, SQLITE_STATIC); // Use SQLITE_STATIC if data is not managed by SQLite
        sqlite3_bind_int(stmt, 2, fingerprintFMDataSize);                                    // Bind the size
        status = sqlite3_step(stmt);
        if (!status)
            handleError("Could not enroll fingerprint.");

        sqlite3_finalize(stmt);

        cout << "Success: Enrolled fingerprint." << endl;
    }
}

int main()
{
    setupFingerprintDevice();
    initialiseDatabase();
    captureAndConvertFingerprint();
    enrollFingerprint();

    dpfpdd_exit();
    _getch();
    return 0;
}
