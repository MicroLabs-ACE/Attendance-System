#include <conio.h>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <string>

#include "dpfj.h"
#include "dpfpdd.h"

using namespace std;

// Function to handle DPFPDD errors
void handleDPFPDDError(int errorCode)
{
    switch (errorCode)
    {
    case DPFPDD_E_FAILURE:
        cout << "DPFPDD_E_FAILURE: Unexpected failure." << endl;
        dpfpdd_exit();
        break;
    case DPFPDD_E_INVALID_DEVICE:
        cout << "DPFPDD_E_INVALID_DEVICE: Invalid reader handle." << endl;
        dpfpdd_exit();
        break;
    case DPFPDD_E_DEVICE_BUSY:
        cout << "DPFPDD_E_DEVICE_BUSY: Another operation is in progress." << endl;
        dpfpdd_exit();
        break;
    case DPFPDD_E_MORE_DATA:
        cout << "DPFPDD_E_MORE_DATA: Insufficient memory is allocated for the image_data. The required size is in the image_size." << endl;
        dpfpdd_exit();
        break;
    case DPFPDD_E_INVALID_PARAMETER:
        cout << "DPFPDD_E_INVALID_PARAMETER: Invalid parameter set in function." << endl;
        dpfpdd_exit();
        break;
    case DPFPDD_E_DEVICE_FAILURE:
        cout << "DPFPDD_E_DEVICE_FAILURE: Failed to start capture, reader is not functioning properly." << endl;
        dpfpdd_exit();
        break;
    default:
        cout << "Unknown error code: " << errorCode << endl;
        dpfpdd_exit();
        break;
    }
}

// Function to create an SQLite database and the fingerprintTable
void create_db()
{
    sqlite3 *db;
    int rc = sqlite3_open("fingerprints.db", &db);

    if (rc)
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    // SQL statement to create the table with an auto-incremented primary key
    const char *createTableSQL = "CREATE TABLE IF NOT EXISTS fingerprintTable (id INTEGER PRIMARY KEY AUTOINCREMENT, binary_data BLOB);";
    rc = sqlite3_exec(db, createTableSQL, 0, 0, 0);

    if (rc)
    {
        cerr << "Error creating table: " << sqlite3_errmsg(db) << endl;
        return;
    }
}

// Function to insert binary data (fingerprint) into the database
void insert_into_table(unsigned char *data, int dataSize)
{
    sqlite3 *db;
    int rc = sqlite3_open("fingerprints.db", &db);

    if (rc)
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    cout << "Opened fingerprints database." << endl;

    const char *insertSQL = "INSERT INTO fingerprintTable (binary_data) VALUES (?);";

    // Prepare the SQL statement
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, insertSQL, -1, &stmt, 0) == SQLITE_OK)
    {
        // Bind the unsigned char array as a BLOB
        sqlite3_bind_blob(stmt, 1, data, dataSize, SQLITE_STATIC); // Use SQLITE_STATIC if data is not managed by SQLite
        int result = sqlite3_step(stmt);
        if (result != SQLITE_DONE)
        {
            cerr << "Error inserting data: " << sqlite3_errmsg(db) << endl;
            return;
        }
        sqlite3_finalize(stmt);
        cout << "Inserted fingerprint data into table successfully." << endl;
    }

    // Close the database when done
    sqlite3_close(db);
}

int main()
{
    // Create the fingerprint database and table
    create_db();

    // Initialize DPFPDD
    int initResult = dpfpdd_init();
    if (initResult != DPFPDD_SUCCESS)
    {
        handleDPFPDDError(initResult);
        return 1;
    }

    DPFPDD_STATUS dpStatus;
    DPFPDD_DEV_INFO devInfoArray[10];
    unsigned int devCount = sizeof(devInfoArray) / sizeof(devInfoArray[0]);
    DPFPDD_DEV devHandle = NULL;

    if (dpStatus != DPFPDD_SUCCESS)
    {
        handleDPFPDDError(dpStatus);
        _getch();
        return 1; // Return 1 on error
    }

    dpStatus = dpfpdd_query_devices(&devCount, devInfoArray);
    if (dpStatus != DPFPDD_SUCCESS)
    {
        handleDPFPDDError(dpStatus);
        return 1; // Return 1 on error
    }

    if (devCount == 0)
    {
        cerr << "No devices found." << endl;
        dpfpdd_exit();
        exit(1); // Exit the program with an error status
    }

    cout << "Found " << devCount << " device(s):" << endl;

    for (unsigned int i = 0; i < devCount; ++i)
        cout << "Device " << i << ": " << devInfoArray[i].name << endl;

    unsigned int devIndex;

    cout << "Enter index of device: ";
    cin >> devIndex;

    // Reader information
    char *readerName = devInfoArray[devIndex].name;
    DPFPDD_DEV readerHandle;
    DPFJ_FMD_FORMAT enrollmentFormat = DPFJ_FMD_DP_REG_FEATURES;

    // Open the fingerprint reader
    int openResult = dpfpdd_open_ext(readerName, DPFPDD_PRIORITY_COOPERATIVE, &readerHandle);
    if (openResult != DPFPDD_SUCCESS)
    {
        handleDPFPDDError(openResult);
        _getch();
        return 1; // Return 1 on error
    }
    else
    {
        cout << "Fingerprint reader opened successfully." << endl;

        // Configure capture parameters
        DPFPDD_CAPTURE_PARAM captureParam = {0};
        captureParam.size = sizeof(captureParam);
        captureParam.image_fmt = DPFPDD_IMG_FMT_ANSI381;
        captureParam.image_proc = DPFPDD_IMG_PROC_NONE;
        captureParam.image_res = 700;

        unsigned int image_size;
        unsigned char *image_data;

        // Allocate memory for image_data (you may need to adjust the size)
        image_size = 1000000; // Adjust this size as needed
        image_data = (unsigned char *)malloc(image_size);

        // Initialize capture result
        DPFPDD_CAPTURE_RESULT captureResult = {0};
        captureResult.size = sizeof(captureResult);
        captureResult.info.size = sizeof(captureResult.info);

        // Capture the fingerprint image
        int captureStatus = dpfpdd_capture(readerHandle, &captureParam, (unsigned int)(-1), &captureResult, &image_size, image_data);
        if (captureStatus != DPFPDD_SUCCESS)
        {
            handleDPFPDDError(captureStatus);
            _getch();
            return 1;
        }

        cout << "Captured fingerprint image." << endl;

        // Convert the captured fingerprint image to FMD format
        DPFJ_FMD_FORMAT fmdFormat = DPFJ_FMD_ANSI_378_2004;
        unsigned char *pFeatures = NULL;
        unsigned int nFeaturesSize = MAX_FMD_SIZE;
        pFeatures = new unsigned char[nFeaturesSize];
        int conversionResult = dpfj_create_fmd_from_fid(captureParam.image_fmt, image_data, image_size, fmdFormat, pFeatures, &nFeaturesSize);

        if (conversionResult != DPFPDD_SUCCESS)
        {
            handleDPFPDDError(captureStatus);
            _getch();
            return 1;
        }

        cout << "Converted fingerprint image to FMD successfully." << endl;

        // Insert the converted fingerprint data into the database
        insert_into_table(pFeatures, nFeaturesSize);
    }

    // Clean up and exit
    dpfpdd_exit();

    _getch(); // Wait for a key press
    return 0;
}
