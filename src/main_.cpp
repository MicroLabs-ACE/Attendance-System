#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include <conio.h>
#include <d3d11.h>
#include <sqlite3.h>
#include <tchar.h>

#include "dpfj.h"
#include "dpfpdd.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

using namespace std;

// Status messages
int rc;

// Fingerprint
DPFPDD_DEV deviceHandle;
unsigned int deviceImageRes;
DPFPDD_IMAGE_FMT FIDFormat = DPFPDD_IMG_FMT_ISOIEC19794;
DPFPDD_IMAGE_PROC FIDProcessing = DPFPDD_IMG_PROC_NONE;
DPFJ_FMD_FORMAT FMDFormat = DPFJ_FMD_ISO_19794_2_2005;

// Database
sqlite3 *db;
string dbINIFilename = "../config/database.ini";
string dbSaveFilePath = "../data/database/";

struct FID // Fingerprint Image Data
{
    unsigned int size = 500000;
    unsigned char *data = (unsigned char *)malloc(size);
};

struct FMD // Fingerprint Minutiae Data
{
    unsigned int size = MAX_FMD_SIZE;
    unsigned char *data = new unsigned char[size];
    bool isEmpty = true;
};

struct Person
{
    string email;
    string firstName;
    string otherName;
    string lastName;
    bool isValid;
};

string getCurrentTimestamp()
{
    // Get the current time
    time_t currentTime = time(nullptr);
    tm *timeInfo = localtime(&currentTime);

    // Format the time
    char formattedTime[20];
    strftime(formattedTime, sizeof(formattedTime), "%d/%m/%y %H:%M:%S", timeInfo);

    // Return the formatted time as a string
    return formattedTime;
}

pair<map<string, string>, map<string, string>> parseDBiniFile(const string &iniFilePath)
{
    ifstream iniFile(iniFilePath);
    if (!iniFile.is_open())
    {
        return {{}, {}};
    }

    map<string, string> database;
    map<string, string> tables;
    string line;
    string currentSection;

    while (getline(iniFile, line))
    {
        if (line.empty() || line[0] == ';')
            continue;
        else
        {
            line = line.substr(line.find_first_not_of(" \t\r\n"));

            if (line[0] == '[' && line[line.length() - 1] == ']')
            {
                currentSection = line.substr(1, line.length() - 2);
            }
            else
            {
                size_t equalPos = line.find('=');
                if (equalPos != string::npos)
                {
                    string key = line.substr(0, equalPos - 1);
                    string value = line.substr(equalPos + 1);
                    key = key.substr(key.find_first_not_of(" \t\r\n"));
                    value = value.substr(value.find_first_not_of(" \t\r\n"));

                    if (currentSection == "Database")
                        database[key] = value;
                    else if (currentSection == "Tables")
                        tables[key] = value;
                }
            }
        }
    }

    iniFile.close();

    return {database, tables};
}

void initialiseDatabase()
{
    map<string, string> database;
    map<string, string> tables;
    tie(database, tables) = parseDBiniFile(dbINIFilename);

    if (database.find("db_name") != database.end())
    {
        string dbName = database.at("db_name");
        dbName += ".db";

        rc = sqlite3_open((dbSaveFilePath + dbName).c_str(), &db);
        if (rc != SQLITE_OK)
        {
            cerr << "Could not initialise database." << endl;
            return;
        }
    }

    for (const auto &pair : tables)
    {
        string createTableSQL = "CREATE TABLE IF NOT EXISTS " + pair.first + " (" + pair.second + ");";
        rc = sqlite3_exec(db, createTableSQL.c_str(), 0, 0, 0);
        if (rc != SQLITE_OK)
        {
            cerr << "Could not create table: " << pair.first << "." << endl;
            return;
        }
    }

    cout << "Initialised database." << endl;
}

void setFingerprintDevice()
{
    // Initialise DPFPDD
    rc = dpfpdd_init();
    if (rc != DPFPDD_SUCCESS)
    {
        cerr << "Could not initialise DPFPDD." << endl;
        return;
    }

    // Query biometric devices connected to device
    DPFPDD_DEV_INFO deviceInfoArray[2];
    unsigned int deviceCount = sizeof(deviceInfoArray) / sizeof(deviceInfoArray[0]);
    rc = dpfpdd_query_devices(&deviceCount, deviceInfoArray);
    if (rc != DPFPDD_SUCCESS)
    {
        cerr << "Could not query devices." << endl;
        return;
    }

    // Check for first device that is not a default biometric device
    string defaultDeviceStr = "&0000&0000";
    unsigned int deviceIndex = 99;
    for (unsigned int i = 0; i < deviceCount; i++)
    {
        string selectDeviceStr = deviceInfoArray[i].name;
        if (selectDeviceStr.find(defaultDeviceStr) == string::npos)
        {
            deviceIndex = i;
            break;
        }
    }

    if (deviceIndex == 99)
    {
        cerr << "Could not set device index." << endl;
        return;
    }

    char *deviceName = deviceInfoArray[deviceIndex].name;
    rc = dpfpdd_open_ext(deviceName, DPFPDD_PRIORITY_EXCLUSIVE, &deviceHandle);
    if (rc != DPFPDD_SUCCESS)
    {
        cerr << "Could not open DPFPDD." << endl;
        return;
    }

    DPFPDD_DEV_CAPS deviceCapabilities;
    deviceCapabilities.size = 100;
    rc = dpfpdd_get_device_capabilities(deviceHandle, &deviceCapabilities);
    if (rc != DPFPDD_SUCCESS)
    {
        cerr << "Could not get device capabilities." << endl;
        return;
    }

    deviceImageRes = deviceCapabilities.resolutions[0];
}

FMD captureAndConvertFingerprint()
{
    DPFPDD_CAPTURE_PARAM captureParam = {0};
    captureParam.size = sizeof(captureParam);
    captureParam.image_fmt = FIDFormat;
    captureParam.image_proc = FIDProcessing;
    captureParam.image_res = deviceImageRes;

    DPFPDD_CAPTURE_RESULT captureResult = {0};
    captureResult.size = sizeof(captureResult);
    captureResult.info.size = sizeof(captureResult.info);

    FID fid;
    rc = dpfpdd_capture(deviceHandle, &captureParam, (unsigned int)(-1), &captureResult, &fid.size, fid.data);
    if (rc != DPFPDD_SUCCESS)
    {
        cerr << "Could not capture fingerprint." << endl;
        return FMD();
    }

    FMD fmd;
    rc = dpfj_create_fmd_from_fid(captureParam.image_fmt, fid.data, fid.size, FMDFormat, fmd.data, &fmd.size);
    if (rc != DPFJ_SUCCESS)
    {
        cerr << "Could not convert fingerprint." << endl;
        return FMD();
    }

    fmd.isEmpty = false;

    return fmd;
}

void insertPerson(Person person)
{
    const char *insertPersonSQL = "INSERT INTO Person (email, first_name, other_name, last_name) VALUES (?, ?, ?, ?)";
    sqlite3_stmt *insertPersonStmt;

    // Prepare the SQL statement
    if (sqlite3_prepare_v2(db, insertPersonSQL, -1, &insertPersonStmt, 0) == SQLITE_OK)
    {
        // Bind parameters
        sqlite3_bind_text(insertPersonStmt, 1, person.email.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(insertPersonStmt, 2, person.firstName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(insertPersonStmt, 3, person.otherName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(insertPersonStmt, 4, person.lastName.c_str(), -1, SQLITE_STATIC);

        // Execute the statement
        rc = sqlite3_step(insertPersonStmt);

        // Check for errors
        if (rc != SQLITE_DONE)
        {
            sqlite3_finalize(insertPersonStmt); // Don't forget to finalize the statement
            return;
        }

        // Finalize the statement
        sqlite3_finalize(insertPersonStmt);
    }

    else
    {
        cerr << "Error preparing SQL statement: " << sqlite3_errmsg(db) << endl;
        return;
    }
}

void insertFingerprint(string email)
{
    FMD fmd = captureAndConvertFingerprint();

    if (!fmd.isEmpty)
    {
        const char *insertFingerprintSQL = "UPDATE Person SET fingerprint_data = ? WHERE email = ?";
        sqlite3_stmt *insertFingerprintStmt;

        if (sqlite3_prepare_v2(db, insertFingerprintSQL, -1, &insertFingerprintStmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_blob(insertFingerprintStmt, 1, fmd.data, fmd.size, SQLITE_STATIC);
            sqlite3_bind_text(insertFingerprintStmt, 2, email.c_str(), -1, SQLITE_STATIC);
        }
        else
            return;

        // Execute the statement
        rc = sqlite3_step(insertFingerprintStmt);

        // Check for errors
        if (rc != SQLITE_DONE)
        {
            sqlite3_finalize(insertFingerprintStmt);
            return;
        }

        // Finalize the statement
        sqlite3_finalize(insertFingerprintStmt);
    }

    else
        return;
}

bool validateInput(string input, string datatype)
{
    bool isValid;
    if (datatype == "TEXT")
        isValid = regex_match(input, regex(R"([a-zA-Z]{2,})"));

    else if (datatype == "EMAIL")
        isValid = regex_match(input, regex(R"([a-zA-Z0-9._%+-]+@oauife\.edu\.ng)"));

    return isValid;
}

Person getPersonInput(Person person = Person())
{
    // Collect inputs
    // Email
    cout << "Enter email address: ";
    if (person.email.empty())
        cin >> person.email;
    else
        cout << person.email << endl;

    // First name
    cout << "Enter first name: ";
    if (person.firstName.empty())
        cin >> person.firstName;
    else
        cout << person.firstName << endl;

    // Other name
    cout << "Enter other name: ";
    if (person.otherName.empty())
        cin >> person.otherName;
    else
        cout << person.otherName << endl;

    // Last name
    cout << "Enter last name: ";
    if (person.lastName.empty())
        cin >> person.lastName;
    else
        cout << person.lastName << endl;

    cout << endl;

    // Checks
    bool isEmail = validateInput(person.email, "EMAIL");
    if (!isEmail)
        person.email = "";

    bool isFirstName = validateInput(person.firstName, "TEXT");
    if (!isFirstName)
        person.firstName = "";

    bool isOtherName = validateInput(person.otherName, "TEXT");
    if (!isOtherName)
        person.otherName = "";

    bool isLastName = validateInput(person.lastName, "TEXT");
    if (!isLastName)
        person.lastName = "";

    person.isValid = isEmail && isFirstName && isOtherName && isLastName;

    return person;
}

int main()
{
    // Person testPerson = getPersonInput();
    // while (!testPerson.isValid)
    // {
    //     testPerson = getPersonInput(testPerson);
    //     if (testPerson.isValid)
    //         cout << testPerson.email << " " << testPerson.firstName << " " << testPerson.otherName << " " << testPerson.lastName << endl;
    // }

    setFingerprintDevice();
    _getch();
}