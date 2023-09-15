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

    // Reader information
    char readerName[] = "$00$05ba&000a&0103{3CDEF154-2A0D-40F9-93B1-3FE8C0765719}";
    DPFPDD_DEV readerHandle;
    DPFJ_FMD_FORMAT enrollmentFormat = DPFJ_FMD_DP_REG_FEATURES;

    // Initialize DPFPDD
    int initResult = dpfpdd_init();
    if (initResult != DPFPDD_SUCCESS)
    {
        handleDPFPDDError(initResult);
        return 1;
    }

    // Open the fingerprint reader
    int openResult = dpfpdd_open_ext(readerName, DPFPDD_PRIORITY_COOPERATIVE, &readerHandle);
    if (openResult != DPFPDD_SUCCESS)
    {
        handleDPFPDDError(openResult);
        return 1; // Return 1 on error
    }
    else
    {
        cout << "Fingerprint reader opened successfully." << endl;

        // Configure capture parameters
        DPFPDD_CAPTURE_PARAM captureParam = {0};
        captureParam.size = sizeof(captureParam);
        captureParam.image_fmt = DPFPDD_IMG_FMT_ISOIEC19794;
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
            return 1;
        }

        cout << "Captured fingerprint image." << endl;

        // Convert the captured fingerprint image to FMD format
        DPFJ_FMD_FORMAT fmdFormat = DPFJ_FMD_ISO_19794_2_2005;
        unsigned char *pFeatures = NULL;
        unsigned int nFeaturesSize = MAX_FMD_SIZE;
        pFeatures = new unsigned char[nFeaturesSize];
        int conversionResult = dpfj_create_fmd_from_fid(captureParam.image_fmt, image_data, image_size, fmdFormat, pFeatures, &nFeaturesSize);

        if (conversionResult != DPFPDD_SUCCESS)
        {
            handleDPFPDDError(captureStatus);
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
