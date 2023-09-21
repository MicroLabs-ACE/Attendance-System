#include <conio.h>
#include <iostream>
#include <sqlite3.h>
#include <string>

#include "dpfj.h"
#include "dpfpdd.h"

using namespace std;


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

    dpfpdd_exit();
    cout << "Press any key to exit!" << endl;
    _getch();
}

void handleDPFJError(int errorCode) {}

void handleSQLiteError(int errorCode) {}

// Function to create an SQLite database and the fingerprintTable
void initialiseDatabase()
{
    sqlite3 *db;
    int rc = sqlite3_open("fingerprints.db", &db);

    if (rc)
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    // SQL statement to create the table with an auto-incremented primary key
    const char *createTableSQL = "CREATE TABLE IF NOT EXISTS fingerprintTable (id INTEGER PRIMARY KEY AUTOINCREMENT, binary_data BLOB, size INTEGER);";
    rc = sqlite3_exec(db, createTableSQL, 0, 0, 0);

    if (rc)
    {
        cerr << "Error creating table: " << sqlite3_errmsg(db) << endl;
        return;
    }
}

// Function to insert binary data (fingerprint) into the database
void insertFingerprint(unsigned char *data, int dataSize)
{
    cout << "Size: " << sizeof(data) << endl;
    sqlite3 *db;
    int rc = sqlite3_open("fingerprints.db", &db);

    if (rc)
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    cout << "Opened fingerprints database." << endl;

    const char *insertSQL = "INSERT INTO fingerprintTable (binary_data, size) VALUES (?, ?);";

    // Prepare the SQL statement
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, insertSQL, -1, &stmt, 0) == SQLITE_OK)
    {
        // Bind the unsigned char array as a BLOB
        sqlite3_bind_blob(stmt, 1, data, dataSize, SQLITE_STATIC); // Use SQLITE_STATIC if data is not managed by SQLite
        sqlite3_bind_int(stmt, 2, dataSize);                       // Bind the size
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

unsigned char **retrieveFingerprints(int &numFingerprints, unsigned int *&fingerprintSizes)
{
    unsigned char **fingerprints = nullptr;
    fingerprintSizes = nullptr;
    numFingerprints = 0;

    sqlite3 *db;
    int rc = sqlite3_open("fingerprints.db", &db);

    if (rc)
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return fingerprints;
    }

    const char *selectSQL = "SELECT binary_data, size FROM fingerprintTable;";
    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, 0);
    if (rc != SQLITE_OK)
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
    rc = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, 0);
    if (rc != SQLITE_OK)
    {
        cerr << "Error preparing SQL statement: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        delete[] fingerprints; // Clean up memory
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
    sqlite3_close(db);

    return fingerprints;
}

int main()
{
    // Create the fingerprint database and table
    initialiseDatabase();

    // Initialize DPFPDD
    int initResult = dpfpdd_init();
    if (initResult != DPFPDD_SUCCESS)
    {
        handleDPFPDDError(initResult);
        return 1;
    }

    cout << "Initialised DPFPDD." << endl;

    DPFPDD_STATUS queryResult;
    DPFPDD_DEV_INFO devInfoArray[10];
    unsigned int devCount = sizeof(devInfoArray) / sizeof(devInfoArray[0]);
    DPFPDD_DEV devHandle = NULL;

    queryResult = dpfpdd_query_devices(&devCount, devInfoArray);
    if (queryResult != DPFPDD_SUCCESS)
    {
        handleDPFPDDError(queryResult);
        return 1;
    }

    cout << "Queried devices." << endl;

    if (devCount == 0)
    {
        cout << "No devices found." << endl;
        _getch();
        dpfpdd_exit();
        return 1;
    }

    cout << "Found " << devCount << " device(s):" << endl;
    for (unsigned int i = 0; i < devCount; ++i)
        cout << "Device " << i << ": " << devInfoArray[i].name << endl;

    unsigned int devIndex;
    cout << "Enter index of device: ";
    cin >> devIndex;

    unsigned int dpi;
    cout << "Enter dpi of device: ";
    cin >> dpi;

    // Reader information
    char *readerName = devInfoArray[devIndex].name;
    DPFPDD_DEV readerHandle;

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

        while (true)
        {
            cout << "Place your finger..." << endl;

            // Configure capture parameters
            DPFPDD_CAPTURE_PARAM captureParam = {0};
            captureParam.size = sizeof(captureParam);
            captureParam.image_fmt = DPFPDD_IMG_FMT_ISOIEC19794;
            captureParam.image_proc = DPFPDD_IMG_PROC_NONE;
            captureParam.image_res = dpi;

            unsigned int image_size;
            unsigned char *image_data;

            // Allocate memory for image_data (you may need to adjust the size)
            image_size = 500000;
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
            unsigned char *fingerprint = NULL;
            unsigned int fingerprintSize = MAX_FMD_SIZE;
            fingerprint = new unsigned char[fingerprintSize];

            int conversionResult = dpfj_create_fmd_from_fid(captureParam.image_fmt, image_data, image_size, fmdFormat, fingerprint, &fingerprintSize);
            if (conversionResult != DPFPDD_SUCCESS)
            {
                handleDPFPDDError(captureStatus);
                return 1;
            }
            cout << "Converted fingerprint image to FMD successfully." << endl;

            char action;
            cout << "Enter action(e for enrol, v for verify, q to quit): ";
            cin >> action;

            if (action == 'e')
            {
                cout << "Enrolling..." << endl;
                insertFingerprint(fingerprint, fingerprintSize);
            }

            else if (action == 'v')
            {
                cout << "Verifying..." << endl;

                int numFingerprints;
                unsigned int *fingerprintSizes;
                unsigned char **allFingerprints = retrieveFingerprints(numFingerprints, fingerprintSizes);

                unsigned int thresholdScore = 5;
                unsigned int candidateCnt = 5;
                DPFJ_CANDIDATE candidates;

                int identifyResult = dpfj_identify(fmdFormat, fingerprint, fingerprintSize, 0, fmdFormat, numFingerprints, allFingerprints, fingerprintSizes, thresholdScore, &candidateCnt, &candidates);
                if (identifyResult != DPFJ_SUCCESS)
                {
                    handleDPFPDDError(identifyResult);
                    return 1;
                }

                cout << "Fingerprint Index: " << candidates.fmd_idx << endl;
            }

            else
                break;
        }
    }

    // Close the fingerprint reader
    dpfpdd_close(devHandle);

    // Clean up and exit
    dpfpdd_exit();

    cout << "Press any key to exit!" << endl;
    _getch(); // Wait for a key press
    return 0;
}
