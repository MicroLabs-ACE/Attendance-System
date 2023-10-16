#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <d3d11.h>
#include <sqlite3.h>
#include <tchar.h>

#include "dpfj.h"
#include "dpfpdd.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

using namespace std;

int rc;
string statusMessage = "";

// Fingerprint device
DPFPDD_DEV deviceHandle;
DPFJ_FMD_FORMAT FMDFormat = DPFJ_FMD_ISO_19794_2_2005;

bool isFingerprintDevice = false;
bool burstMode = false;

// Database
sqlite3 *db;

struct Person
{
    string first_name;
    string last_name;
    string id;
    string logTimestamp;
};

struct VerifiableFingerprintData
{
    vector<string> personIDs;
    unsigned int numberOfFingerprints;
    unsigned int *fingerprintSizes;
    unsigned char **fingerprints;
};

// struct EventData
// {
//     string name;
//     string start_time;
//     string end_time;
// };

struct EventData
{
    string name;
    vector<Person> persons;
};

EventData currEventData;

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

// Data
static ID3D11Device *g_pd3dDevice = nullptr;
static ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;
static IDXGISwapChain *g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

string getCurrentTimestamp()
{
    // Get the current time
    time_t currentTime = time(nullptr);
    tm *timeInfo = localtime(&currentTime);

    // Format the time as "dd/mm/yy hh:mm:ss"
    char formattedTime[20]; // Sufficient size to hold the formatted time
    strftime(formattedTime, sizeof(formattedTime), "%d/%m/%y %H:%M:%S", timeInfo);

    // Return the formatted time as a string
    return formattedTime;
}

void getPersons()
{
    const char *getPersonsSQL = "SELECT id, first_name, last_name FROM Person;";
    sqlite3_stmt *getPersonsStmt;

    if (sqlite3_prepare_v2(db, getPersonsSQL, -1, &getPersonsStmt, 0) == SQLITE_OK)
    {
        while (sqlite3_step(getPersonsStmt) == SQLITE_ROW)
        {
            const char *id = reinterpret_cast<const char *>(sqlite3_column_text(getPersonsStmt, 0));
            const char *fname = reinterpret_cast<const char *>(sqlite3_column_text(getPersonsStmt, 1));
            const char *lname = reinterpret_cast<const char *>(sqlite3_column_text(getPersonsStmt, 2));
            if (id)
            {
                Person p;
                p.first_name = fname;
                p.last_name = lname;
                p.id = id;
                currEventData.persons.push_back(p);
            }
        }
    }
}

void setFingerprintDevice()
{
    // Initialise DPFPDD
    rc = dpfpdd_init();
    if (rc != DPFPDD_SUCCESS)
    {
        statusMessage = "Could not initialise DPFPDD.";
        return;
    }

    // Query biometric devices connected to device
    DPFPDD_DEV_INFO deviceInfoArray[2];
    unsigned int deviceCount = sizeof(deviceInfoArray) / sizeof(deviceInfoArray[0]);
    rc = dpfpdd_query_devices(&deviceCount, deviceInfoArray);

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
        statusMessage = "No fingerprint device found.";
        return;
    }

    char *deviceName = deviceInfoArray[deviceIndex].name;
    rc = dpfpdd_open_ext(deviceName, DPFPDD_PRIORITY_EXCLUSIVE, &deviceHandle);
    if (rc != DPFPDD_SUCCESS)
    {

        statusMessage = "Could not open fingerprint device.";
        return;
    }

    isFingerprintDevice = true;
    statusMessage = "Initialised and opened fingerprint device.";
}

FMD captureAndConvertFingerprint()
{
    statusMessage = "Place your finger...";

    DPFPDD_CAPTURE_PARAM captureParam = {0};
    captureParam.size = sizeof(captureParam);
    captureParam.image_fmt = DPFPDD_IMG_FMT_ISOIEC19794;
    captureParam.image_proc = DPFPDD_IMG_PROC_NONE;
    captureParam.image_res = 700;

    DPFPDD_CAPTURE_RESULT captureResult = {0};
    captureResult.size = sizeof(captureResult);
    captureResult.info.size = sizeof(captureResult.info);

    FID fid;
    rc = dpfpdd_capture(deviceHandle, &captureParam, (unsigned int)(-1), &captureResult, &fid.size, fid.data);
    if (rc != DPFPDD_SUCCESS)
    {
        statusMessage = "Could not capture fingerprint.";
        return FMD();
    }

    FMD fmd;
    rc = dpfj_create_fmd_from_fid(captureParam.image_fmt, fid.data, fid.size, FMDFormat, fmd.data, &fmd.size);
    if (rc != DPFJ_SUCCESS)
    {
        statusMessage = "Could not convert fingerprint.";
        return FMD();
    }

    statusMessage = "Captured and converted fingerprint.";

    fmd.isEmpty = false;

    return fmd;
}

void enrol(Person person)
{
    const char *insertPersonSQL = "INSERT INTO Person (id, first_name, last_name) VALUES (?, ?, ?);";
    sqlite3_stmt *insertPersonStmt;

    // Prepare the SQL statement
    if (sqlite3_prepare_v2(db, insertPersonSQL, -1, &insertPersonStmt, 0) == SQLITE_OK)
    {
        // Bind parameters
        sqlite3_bind_text(insertPersonStmt, 1, person.id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(insertPersonStmt, 2, person.first_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(insertPersonStmt, 3, person.last_name.c_str(), -1, SQLITE_STATIC);

        // Execute the statement
        rc = sqlite3_step(insertPersonStmt);

        // Check for errors
        if (rc != SQLITE_DONE)
        {
            statusMessage = "Error inserting data: " + string(sqlite3_errmsg(db));
            sqlite3_finalize(insertPersonStmt); // Don't forget to finalize the statement
            return;
        }

        // Finalize the statement
        sqlite3_finalize(insertPersonStmt);
    }
    else
    {
        statusMessage = "Error preparing statement: " + string(sqlite3_errmsg(db));
        return;
    }

    statusMessage = "Inserted person.";
}

void insertFingerprint(string id)
{
    FMD fmd = captureAndConvertFingerprint();

    if (!fmd.isEmpty)
    {
        const char *insertFingerprintSQL = "UPDATE Person SET fingerprintData = ? WHERE id = ?";
        sqlite3_stmt *insertFingerprintStmt;

        if (sqlite3_prepare_v2(db, insertFingerprintSQL, -1, &insertFingerprintStmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_blob(insertFingerprintStmt, 1, fmd.data, fmd.size, SQLITE_STATIC);
            sqlite3_bind_int(insertFingerprintStmt, 2, fmd.size);
            sqlite3_bind_text(insertFingerprintStmt, 3, id.c_str(), -1, SQLITE_STATIC);
        }
        else
        {
            statusMessage = "Error preparing statement: " + string(sqlite3_errmsg(db));
            return;
        }

        // Execute the statement
        rc = sqlite3_step(insertFingerprintStmt);

        // Check for errors
        if (rc != SQLITE_DONE)
        {
            statusMessage = "Error inserting data: " + string(sqlite3_errmsg(db));
            sqlite3_finalize(insertFingerprintStmt); // Don't forget to finalize the statement
            return;
        }

        // Finalize the statement
        sqlite3_finalize(insertFingerprintStmt);
    }

    else
    {
        statusMessage = "No fingerprint was gotten.";
        return;
    }

    statusMessage = "Inserted fingerprint.";
}

FMD retrieveFingerprint(string id)
{
    FMD fmd;

    const char *retrieveFingerprintSQL = "SELECT fingerprintData FROM Person WHERE id = ?";
    sqlite3_stmt *retrieveFingerprintStmt;
    if (sqlite3_prepare_v2(db, retrieveFingerprintSQL, -1, &retrieveFingerprintStmt, 0) == SQLITE_OK)
    {
        sqlite3_bind_text(retrieveFingerprintStmt, 1, id.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(retrieveFingerprintStmt) == SQLITE_ROW)
        {
            const void *data = sqlite3_column_blob(retrieveFingerprintStmt, 0);
            int size = sqlite3_column_bytes(retrieveFingerprintStmt, 0);

            if (data && size > 0)
            {
                const unsigned char *fingerprintData = static_cast<const unsigned char *>(data);
                unsigned char *fingerprint = new unsigned char[size];
                memcpy(fingerprint, fingerprintData, size);
                fmd.data = fingerprint;
                fmd.size = size;
            }
            else
            {
                cerr << "No fingerprint found for the id supplied." << endl;
                sqlite3_finalize(retrieveFingerprintStmt);
                return fmd;
            }
        }
    }

    sqlite3_finalize(retrieveFingerprintStmt);

    cout << "Retrieved fingerprint." << endl;

    fmd.isEmpty = false;
    return fmd;
}

void verifyFingerprint()
{
    while (burstMode)
    {
        VerifiableFingerprintData vfd;
        vfd.numberOfFingerprints = size(currEventData.persons);
        if (vfd.numberOfFingerprints == 0)
        {
            cerr << "No person added to event." << endl;
            return;
        }

        else
        {
            vfd.fingerprints = new unsigned char *[vfd.numberOfFingerprints];
            vfd.fingerprintSizes = new unsigned int[vfd.numberOfFingerprints];

            for (const auto &person : currEventData.persons)
            {
                vfd.personIDs.push_back(person.id);
            }

            for (const auto &id : vfd.personIDs)
            {
                cout << id << endl;
            }

            int index = 0;
            for (const string &id : vfd.personIDs)
            {
                FMD fmd = retrieveFingerprint(id);
                vfd.fingerprints[index] = fmd.data;
                vfd.fingerprintSizes[index] = fmd.size;

                index++;
            }

            FMD verifyFMD = captureAndConvertFingerprint();

            unsigned int thresholdScore = 5;
            unsigned int candidateCount = 1;
            DPFJ_CANDIDATE candidate;
            rc = dpfj_identify(FMDFormat, verifyFMD.data, verifyFMD.size, 0, FMDFormat, vfd.numberOfFingerprints, vfd.fingerprints, vfd.fingerprintSizes, thresholdScore, &candidateCount, &candidate);

            if (rc != DPFJ_SUCCESS)
            {
                cerr << "Could not identify fingerprint." << endl;
                return;
            }

            cout << "Found " << candidateCount << " fingerprints matching finger." << endl;
            if (candidateCount > 0)
            {

                cout << "Person ID: " << vfd.personIDs[candidate.fmd_idx] << endl;
                currEventData.persons[candidate.fmd_idx].logTimestamp = getCurrentTimestamp();
            }
        }
    }
    cout << "Exited verify loop." << endl;
}

pair<map<string, string>, map<string, string>> parseINIFile(const string &iniFilePath)
{
    ifstream iniFile(iniFilePath);
    if (!iniFile.is_open())
    {
        statusMessage = "Failed to open INI file";
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

    statusMessage = "Initialised database INI file.";
    return {database, tables};
}

void initialiseDatabase()
{
    map<string, string> database;
    map<string, string> tables;
    tie(database, tables) = parseINIFile("database.ini");

    if (database.find("db_name") != database.end())
    {
        string dbName = database.at("db_name");
        dbName += ".db";

        rc = sqlite3_open(dbName.c_str(), &db);
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

// Function to write a vector of Person objects to a CSV file
void writeDataToCSV()
{
    // Open the CSV file for writing
    std::ofstream outputFile(currEventData.name + ".csv");

    // Check if the file was opened successfully
    if (!outputFile.is_open())
    {
        std::cerr << "Failed to open file for writing." << std::endl;
        return;
    }

    // Write the header row to the CSV file
    outputFile << "First Name,Last Name,Email,Sign In Time" << std::endl;

    // Write each person's data to the CSV file
    for (const Person &person : currEventData.persons)
    {
        if (!person.logTimestamp.empty())
        {
            outputFile << person.first_name << "," << person.last_name << ","
                       << person.id << "," << person.logTimestamp << std::endl;
        }
    }

    // Close the CSV file
    outputFile.close();
}

// Main code
int main()
{
    setFingerprintDevice();
    initialiseDatabase();
    getPersons();

    // Create application window
    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr};
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Biometric Attendance System", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin("Main Window");
            static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
            ImGui::BeginChild("Enrol", ImVec2(500, 0), true);
            {
                static char first_name_input[256] = "";
                ImGui::InputText("First Name", first_name_input, IM_ARRAYSIZE(first_name_input));

                static char last_name_input[256] = "";
                ImGui::InputText("Last Name", last_name_input, IM_ARRAYSIZE(last_name_input));

                static char email_input[256] = "";
                ImGui::InputText("Email", email_input, IM_ARRAYSIZE(email_input));
                ImGui::Text(statusMessage.c_str());

                // if (ImGui::Button("Capture fingerprint"))
                // {

                // }
                // ImGui::SameLine();

                // Submit button
                if (ImGui::Button("Submit"))
                {
                    // Print values to console and reset input fields if they are not empty
                    std::string firstName = first_name_input;
                    std::string lastName = last_name_input;
                    std::string email = email_input;

                    if (!firstName.empty() && !lastName.empty() && !email.empty())
                    {
                        Person person;
                        person.first_name = firstName;
                        person.last_name = lastName;
                        person.id = email;

                        thread enrThr(enrol, person);
                        enrThr.detach();

                        thread insFngrprntThr(insertFingerprint, person.id);
                        insFngrprntThr.detach();

                        currEventData.persons.push_back(person);

                        // Reset input fields to empty
                        strcpy(first_name_input, "");
                        strcpy(last_name_input, "");
                        strcpy(email_input, "");
                    }
                }

                // ImGui::SameLine();

                if (ImGui::BeginTable("Enrol", 3, flags))
                {
                    // Display headers
                    ImGui::TableSetupColumn("First Name");
                    ImGui::TableSetupColumn("Last Name");
                    ImGui::TableSetupColumn("Email");
                    ImGui::TableHeadersRow();

                    // Populate the table with test data
                    for (const auto &row : currEventData.persons)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", row.first_name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", row.last_name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", row.id.c_str());
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("Event Detail", ImVec2(0, 0), true);
            {
                static char event_name_input[256] = "";
                ImGui::InputText("Event name", event_name_input, IM_ARRAYSIZE(event_name_input));

                static char last_scanned_input[256] = "";
                ImGui::InputText("Last scanned", last_scanned_input, IM_ARRAYSIZE(last_scanned_input));

                if (ImGui::Button("Burst Mode"))
                {
                    burstMode = !burstMode;
                    if (burstMode)
                    {
                        thread vrfyFngrprntThr(verifyFingerprint);
                        vrfyFngrprntThr.detach();
                    }

                    if (!string(event_name_input).empty())
                    {
                        currEventData.name = event_name_input;
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button("End Event"))
                {
                    if (!string(event_name_input).empty())
                    {
                        std::cout << "Event Ended" << std::endl;
                        currEventData.name = event_name_input;

                        if (!currEventData.name.empty())
                        {
                            thread saveEvntThr(writeDataToCSV);
                            saveEvntThr.detach();
                        }
                    }
                }

                if (ImGui::BeginTable("Event Table", 4, flags))
                {
                    // Display headers
                    ImGui::TableSetupColumn("First Name");
                    ImGui::TableSetupColumn("Last Name");
                    ImGui::TableSetupColumn("Email");
                    ImGui::TableSetupColumn("Sign In Time");
                    ImGui::TableHeadersRow();

                    // Populate the table with test data
                    for (const auto &row : currEventData.persons)
                    {
                        if (!row.logTimestamp.empty())

                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", row.first_name.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", row.last_name.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", row.id.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", row.logTimestamp.c_str());
                        }
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::EndChild();
            ImGui::End();
        }
        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

void CreateRenderTarget()
{
    ID3D11Texture2D *pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
