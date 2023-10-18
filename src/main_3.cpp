// Iteration 0

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <regex>
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

// SECTION: Utility variables and functions
// Multi-function variables
int rc; // Return code (used to store the return code of functions)

enum STATE
{
    IDLE,  // Initial state
    ENROL, // State for enrolling fingerprints
    VERIFY // State for verifying fingerprints
};

STATE currentState = IDLE; // Set the initial state to IDLE

size_t numberOfFingerprints; // Store the number of fingerprints

string statusMessage; // Store status messages

// Utilities functions
/**
 * @brief Get the current timestamp in a specific format.
 *
 * This function retrieves the current time and formats it as a timestamp in the "dd/mm/yy HH:MM:SS" format.
 *
 * @return A string containing the current timestamp.
 */
string getCurrentTimestamp()
{
    // Get the current time
    time_t currentTime = time(nullptr);
    tm *timeInfo = localtime(&currentTime);

    // Format the time
    char formattedTime[20];
    strftime(formattedTime, sizeof(formattedTime), "%d/%m/%y %H:%M:%S", timeInfo);

    return formattedTime;
}

/**
 * @brief Validates input based on the specified data type.
 *
 * This function checks whether the provided input conforms to a specific data type.
 *
 * @param input The input string to be validated.
 * @param datatype The data type to which the input should conform (e.g., "TEXT" or "EMAIL").
 *
 * @return true if the input is valid for the given data type; false otherwise.
 */
bool validateInput(string input, string datatype)
{
    bool isValid;

    // Check the data type to determine the validation criteria.
    if (datatype == "TEXT")
    {
        /**
         * For "TEXT" data type, validate that the input consists of at least 2 or more alphabetic characters.
         * @regex ([a-zA-Z]{2,})
         */
        isValid = regex_match(input, regex(R"([a-zA-Z]{2,})"));
    }
    else if (datatype == "EMAIL")
    {
        /**
         * For "EMAIL" data type, validate that the input matches a specific email pattern (checking if it's an OAU Ife email address).
         * @regex ([a-zA-Z0-9._%+-]+@oauife\.edu\.ng)
         */
        isValid = regex_match(input, regex(R"([a-zA-Z0-9._%+-]+@oauife\.edu\.ng)"));
    }

    return isValid;
}

/**
 * @brief Run a function in a separate thread.
 *
 * This function runs the provided function in a new thread and detaches the thread.
 *
 * @param threadFunction A function object (lambda function or function pointer) to be executed in the new thread.
 *
 * @note The thread is detached, which means it runs independently and doesn't need to be explicitly joined.
 */
void runFunctionInThread(std::function<void()> threadFunction)
{
    // Create a new thread and execute the provided function.
    std::thread threadObject([threadFunction]()
                             { threadFunction(); });

    // Detach the thread to run it independently.
    threadObject.detach();
}

// SECTION: Fingerprint device parameters
/**
 * @brief Fingerprint device initialisation and management.
 */
DPFPDD_DEV fingerprintDeviceHandle;                      ///< Handle to the fingerprint device.
unsigned int fingerprintDeviceImageRes;                  ///< Image resolution of the fingerprint device.
DPFPDD_IMAGE_FMT FIDFormat = DPFPDD_IMG_FMT_ISOIEC19794; ///< Fingerprint image format.
DPFPDD_IMAGE_PROC FIDProcessing = DPFPDD_IMG_PROC_NONE;  ///< Fingerprint image processing.
DPFJ_FMD_FORMAT FMDFormat = DPFJ_FMD_ISO_19794_2_2005;   ///< Fingerprint minutiae data format.

/**
 * @struct FID
 * @brief Fingerprint Image Data
 */
struct FID
{
    unsigned int size = 500000;                          ///< Size of the fingerprint image data.
    unsigned char *data = (unsigned char *)malloc(size); ///< Pointer to the fingerprint image data.
};

/**
 * @struct FMD
 * @brief Fingerprint Minutiae Data
 */
struct FMD
{
    unsigned int size = MAX_FMD_SIZE;              ///< Size of the fingerprint minutiae data.
    unsigned char *data = new unsigned char[size]; ///< Pointer to the fingerprint minutiae data.
    bool isEmpty = true;                           ///< Flag indicating whether the fingerprint minutiae data is empty.
};

FMD currentFMD; ///< Current fingerprint minutiae data.

/**
 * @brief Initializes the fingerprint device and retrieves device information.
 *
 * This function initializes the fingerprint device, queries device information,
 * selects a valid fingerprint device, opens the device, and retrieves device capabilities.
 * It sets the image resolution and updates the status message accordingly.
 *
 * @note Ensure that the DPFPDD library is properly configured and available.
 */
void initialiseFingerprintDevice()
{
    // Initialize DPFPDD
    rc = dpfpdd_init();

    // Check for initialization errors
    if (rc != DPFPDD_SUCCESS)
    {
        statusMessage = "InitialiseDPFPDDError";
        return;
    }

    // Query fingerprint devices connected to the system
    DPFPDD_DEV_INFO fingerprintDeviceInfoArray[2];
    unsigned int fingerprintDeviceCount = sizeof(fingerprintDeviceInfoArray) / sizeof(fingerprintDeviceInfoArray[0]);
    rc = dpfpdd_query_devices(&fingerprintDeviceCount, fingerprintDeviceInfoArray);

    // Check for query errors
    if (rc != DPFPDD_SUCCESS)
    {
        statusMessage = "QueryFingerprintDevicesError";
        return;
    }

    // Check if a valid fingerprint device is available
    string defaultFingerprintDeviceStr = "&0000&0000";
    unsigned int fingerprintDeviceIndex = 99;
    for (unsigned int i = 0; i < fingerprintDeviceCount; i++)
    {
        string selectFingerprintDeviceStr = fingerprintDeviceInfoArray[i].name;
        if (selectFingerprintDeviceStr.find(defaultFingerprintDeviceStr) == string::npos)
        {
            fingerprintDeviceIndex = i;
            break;
        }
    }

    // Check if a valid fingerprint device was found
    if (fingerprintDeviceIndex == 99)
    {
        statusMessage = "FingerprintDeviceIndexError";
        return;
    }

    // Open the fingerprint device
    char *fingerprintDeviceName = fingerprintDeviceInfoArray[fingerprintDeviceIndex].name;
    rc = dpfpdd_open_ext(fingerprintDeviceName, DPFPDD_PRIORITY_EXCLUSIVE, &fingerprintDeviceHandle);

    // Check for errors while opening device
    if (rc != DPFPDD_SUCCESS)
    {
        statusMessage = "OpenDPFPDDError";
        return;
    }

    // Retrieve the device's capabilities and set the image resolution
    DPFPDD_DEV_CAPS fingerprintDeviceCapabilities;
    fingerprintDeviceCapabilities.size = 100;
    rc = dpfpdd_get_device_capabilities(fingerprintDeviceHandle, &fingerprintDeviceCapabilities);

    // Check for device capabilities retrieval errors
    if (rc != DPFPDD_SUCCESS)
    {
        statusMessage = "FingerprintDeviceCapabilitiesError";
        return;
    }
    fingerprintDeviceImageRes = fingerprintDeviceCapabilities.resolutions[0];

    statusMessage = "Initialized fingerprint device.";
}

/**
 * @brief Captures and converts fingerprints from the fingerprint device.
 *
 * This function continuously captures and converts fingerprints from the connected fingerprint device.
 * It checks the device's status, captures fingerprints, converts them to minutiae data, and updates the status message accordingly.
 */
void captureAndConvertFingerprint()
{
    while (true)
    {
        // Check if fingerprint device is connected
        DPFPDD_DEV_STATUS fingerprintDeviceStatus;
        rc = dpfpdd_get_device_status(fingerprintDeviceHandle, &fingerprintDeviceStatus);
        if (rc != DPFPDD_SUCCESS)
        {
            statusMessage = "FingerprintDeviceUnavailable";
        }

        else
        {
            // Set parameters for fingerprint capture
            DPFPDD_CAPTURE_PARAM captureParam = {0};
            captureParam.size = sizeof(captureParam);
            captureParam.image_fmt = FIDFormat;
            captureParam.image_proc = FIDProcessing;
            captureParam.image_res = fingerprintDeviceImageRes;

            DPFPDD_CAPTURE_RESULT captureResult = {0};
            captureResult.size = sizeof(captureResult);
            captureResult.info.size = sizeof(captureResult.info);

            statusMessage = "Place your finger...";

            FID fid;
            rc = dpfpdd_capture(fingerprintDeviceHandle, &captureParam, (unsigned int)(-1), &captureResult, &fid.size, fid.data);
            if (rc != DPFPDD_SUCCESS)
            {
                statusMessage = "FingerprintCaptureError";
            }

            else
            {
                // Convert FID to FMD
                currentFMD = FMD();
                rc = dpfj_create_fmd_from_fid(captureParam.image_fmt, fid.data, fid.size, FMDFormat, currentFMD.data, &currentFMD.size);
                if (rc != DPFJ_SUCCESS)
                {
                    statusMessage = "FingerprintConversionError.";
                }

                else
                {
                    statusMessage = "Captured and converted fingerprint.";
                    currentFMD.isEmpty = false;

                    if (currentState == ENROL)
                    {
                        statusMessage = "Enrolled fingerprint.";
                    }

                    else if (currentState == VERIFY)
                    {
                        statusMessage = "Verified fingerprint.";
                    }
                }
            }
        }
    }
}

// SECTION: Database initialisation and management
/**
 * @brief Database setup and INI file parsing.
 */

sqlite3 *db;                                     ///< SQLite3 database handle.
string dbINIFilename = "../config/database.ini"; ///< Path to the database INI file.
string dbSaveFilePath = "../data/database/";     ///< Path to the database save file directory.

// Helper function
/**
 * @brief Parse an INI file and extract database and table information.
 *
 * This function reads an INI file and extracts database and table configuration information.
 *
 * @param iniFilePath The path to the INI file to be parsed.
 * @return A pair of maps containing database and table configurations.
 */
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

/**
 * @brief Initialize the database and tables based on configuration from an INI file.
 *
 * This function initializes the SQLite database and creates tables based on the configuration obtained from an INI file.
 */
void initialiseDatabase()
{
    // Get initialisation database
    map<string, string> database;
    map<string, string> tables;
    tie(database, tables) = parseDBiniFile(dbINIFilename);

    if (database.empty() || tables.empty())
    {
        statusMessage = "DatabaseInitialisationError";
        return;
    }

    // Create database
    if (database.find("db_name") != database.end())
    {
        string dbName = database.at("db_name");
        dbName += ".db";

        rc = sqlite3_open((dbSaveFilePath + dbName).c_str(), &db);
        if (rc != SQLITE_OK)
        {
            statusMessage = "DatabaseCreationError";
            return;
        }
    }

    // Create database tables
    for (const auto &pair : tables)
    {
        string createTableSQL = "CREATE TABLE IF NOT EXISTS " + pair.first + " (" + pair.second + ");";
        rc = sqlite3_exec(db, createTableSQL.c_str(), 0, 0, 0);
        if (rc != SQLITE_OK)
        {
            statusMessage = "TableCreationError: " + pair.first;
            return;
        }
    }

    statusMessage = "Initialised database.";
}

// SECTION: Person
struct Person
{
    string email;
    string firstName;
    string otherName;
    string lastName;
    bool isValid;
};
Person currentPerson;

/**
 * @brief Validate and sanitize the input data of a Person.
 *
 * This function takes a Person struct as input, validates the email, first name, other name, and last name fields,
 * and updates them if necessary. It also sets the `isValid` flag to true if all fields are valid, or false if any field is invalid.
 *
 * @param person The input Person to be validated and sanitized.
 * @return The validated and sanitized Person with the `isValid` flag set.
 */
Person validatePersonInput(Person person)
{
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

    // Set the `isValid` flag based on all field validations
    person.isValid = isEmail && isFirstName && isOtherName && isLastName;

    return person;
}

/**
 * @brief Insert a Person into the SQLite database.
 *
 * This function prepares an SQL statement for inserting a Person's data into an SQLite database.
 *
 * @param personToInsert The Person object to be inserted into the database.
 */
void enrolPerson()
{
    Person personToInsert = validatePersonInput(currentPerson);
    if (personToInsert.isValid)
    {
        const char *insertPersonSQL = "INSERT INTO Person (email, first_name, other_name, last_name) VALUES (?, ?, ?, ?)";
        sqlite3_stmt *insertPersonStmt;

        // Prepare the SQL statement
        if (sqlite3_prepare_v2(db, insertPersonSQL, -1, &insertPersonStmt, 0) == SQLITE_OK)
        {
            // Bind parameters
            sqlite3_bind_text(insertPersonStmt, 1, personToInsert.email.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertPersonStmt, 2, personToInsert.firstName.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertPersonStmt, 3, personToInsert.otherName.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertPersonStmt, 4, personToInsert.lastName.c_str(), -1, SQLITE_STATIC);

            // Execute the statement
            rc = sqlite3_step(insertPersonStmt);

            // Check for errors
            if (rc != SQLITE_DONE)
            {
                statusMessage = "InsertPerson: SQLiteError";
                sqlite3_finalize(insertPersonStmt);
                return;
            }

            // Finalize the statement
            sqlite3_finalize(insertPersonStmt);
        }

        else
        {
            statusMessage = "EnrolPerson: SQLiteError(" + string(sqlite3_errmsg(db)) + ")";
            return;
        }

        statusMessage = "Enrolled person.";
    }

    else
    {
        statusMessage = "EnrolPersonError";
        return;
    }
}

void enrolFingerprint()
{
    string emailToInsert = currentPerson.email;
    bool isValidEmail = validateInput(emailToInsert, "EMAIL");
    if (!isValidEmail)
    {
        statusMessage = "EnrolFingerprint: InvalidIDError";
        return;
    }

    if (!currentFMD.isEmpty)
    {
        const char *insertFingerprintSQL = "UPDATE Person SET fingerprint_data = ? WHERE email = ?";
        sqlite3_stmt *insertFingerprintStmt;

        if (sqlite3_prepare_v2(db, insertFingerprintSQL, -1, &insertFingerprintStmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_blob(insertFingerprintStmt, 1, currentFMD.data, currentFMD.size, SQLITE_STATIC);
            sqlite3_bind_text(insertFingerprintStmt, 2, emailToInsert.c_str(), -1, SQLITE_STATIC);
        }

        else
        {
            statusMessage = "EnrolFingerprint: SQLiteError(StatementPreparationError)";
            return;
        }

        // Execute the statement
        rc = sqlite3_step(insertFingerprintStmt);

        // Check for errors
        if (rc != SQLITE_DONE)
        {
            sqlite3_finalize(insertFingerprintStmt);
            statusMessage = "EnrolFingerprint: SQLiteError(StatementFinalisationError)";

            return;
        }

        sqlite3_finalize(insertFingerprintStmt);
        statusMessage = "Enrolled fingerprint.";
    }

    currentState = IDLE;
}

// SECTION: Event

// SECTION: ImGui
// Global GUI variables
static ID3D11Device *g_pDirect3DDevice = nullptr;
static ID3D11DeviceContext *g_pDirect3DDeviceContext = nullptr;
static IDXGISwapChain *g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;

// Render target
void CreateRenderTarget()
{
    ID3D11Texture2D *pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pDirect3DDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
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

bool CreateDeviceD3D(HWND windowHandle)
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
    sd.OutputWindow = windowHandle;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pDirect3DDevice, &featureLevel, &g_pDirect3DDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pDirect3DDevice, &featureLevel, &g_pDirect3DDeviceContext);
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
    if (g_pDirect3DDeviceContext)
    {
        g_pDirect3DDeviceContext->Release();
        g_pDirect3DDeviceContext = nullptr;
    }
    if (g_pDirect3DDevice)
    {
        g_pDirect3DDevice->Release();
        g_pDirect3DDevice = nullptr;
    }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND windowHandle, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND windowHandle, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(windowHandle, msg, wParam, lParam))
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

    return ::DefWindowProcW(windowHandle, msg, wParam, lParam);
}

int main()
{
    runFunctionInThread(initialiseDatabase);
    runFunctionInThread(initialiseFingerprintDevice);
    runFunctionInThread(captureAndConvertFingerprint);

    // Create application window
    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"Biometric Attendance System", nullptr};
    ::RegisterClassExW(&wc);
    HWND windowHandle = ::CreateWindowW(wc.lpszClassName, L"Biometric Attendance System", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(windowHandle))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(windowHandle, SW_SHOWDEFAULT);
    ::UpdateWindow(windowHandle);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(windowHandle);
    ImGui_ImplDX11_Init(g_pDirect3DDevice, g_pDirect3DDeviceContext);

    // Our state
    ImVec4 colour = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        {
            ImGui::Begin("Main Window");
            ImGui::BeginChild("Enrol", ImVec2(400, 400), true);
            {
                static char firstNameInput[256] = "";
                ImGui::InputText("First Name", firstNameInput, IM_ARRAYSIZE(firstNameInput));

                static char otherNameInput[256] = "";
                ImGui::InputText("Other Name", otherNameInput, IM_ARRAYSIZE(otherNameInput));

                static char lastNameInput[256] = "";
                ImGui::InputText("Last Name", lastNameInput, IM_ARRAYSIZE(lastNameInput));

                static char emailInput[256] = "";
                ImGui::InputText("Email", emailInput, IM_ARRAYSIZE(emailInput));
                ImGui::Text(statusMessage.c_str());

                if (ImGui::Button("Submit"))
                {
                    currentPerson = Person();
                    currentPerson.email = string(emailInput);
                    currentPerson.firstName = string(firstNameInput);
                    currentPerson.otherName = string(otherNameInput);
                    currentPerson.lastName = string(lastNameInput);

                    runFunctionInThread(enrolPerson);
                }

                if (ImGui::Button("Enrol"))
                {
                    currentPerson = Person();
                    currentPerson.email = string(emailInput);
                    currentPerson.firstName = string(firstNameInput);
                    currentPerson.otherName = string(otherNameInput);
                    currentPerson.lastName = string(lastNameInput);

                    runFunctionInThread(enrolPerson);
                    currentState = ENROL;
                }
            }

            ImGui::EndChild();

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        const float colourWithAlpha[4] = {colour.x * colour.w, colour.y * colour.w, colour.z * colour.w, colour.w};
        g_pDirect3DDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pDirect3DDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, colourWithAlpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(windowHandle);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}