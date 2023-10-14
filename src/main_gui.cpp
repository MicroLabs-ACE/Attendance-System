#include <chrono>
#include <fstream>
#include <functional>
#include <future>
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

// Status messages
int rc;

// Fingerprint
DPFPDD_DEV fingerprintDeviceHandle;
unsigned int fingerprintDeviceImageRes;
DPFPDD_IMAGE_FMT FIDFormat = DPFPDD_IMG_FMT_ISOIEC19794;
DPFPDD_IMAGE_PROC FIDProcessing = DPFPDD_IMG_PROC_NONE;
DPFJ_FMD_FORMAT FMDFormat = DPFJ_FMD_ISO_19794_2_2005;

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

void setFingerprintDevice()
{
    // Initialise DPFPDD
    rc = dpfpdd_init();
    if (rc != DPFPDD_SUCCESS)
    {
        cerr << "Could not initialise DPFPDD." << endl;
        return;
    }

    // Query fingerprint devices connected to device
    DPFPDD_DEV_INFO fingerprintDeviceInfoArray[2];
    unsigned int fingerprintDeviceCount = sizeof(fingerprintDeviceInfoArray) / sizeof(fingerprintDeviceInfoArray[0]);
    rc = dpfpdd_query_devices(&fingerprintDeviceCount, fingerprintDeviceInfoArray);
    if (rc != DPFPDD_SUCCESS)
    {
        cerr << "Could not query fingerprint devices." << endl;
        return;
    }

    // Check for if fingerprint device is a system device
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

    if (fingerprintDeviceIndex == 99)
    {
        cerr << "Could not set fingerprint device index." << endl;
        return;
    }

    char *fingerprintDeviceName = fingerprintDeviceInfoArray[fingerprintDeviceIndex].name;
    rc = dpfpdd_open_ext(fingerprintDeviceName, DPFPDD_PRIORITY_EXCLUSIVE, &fingerprintDeviceHandle);
    if (rc != DPFPDD_SUCCESS)
    {
        cerr << "Could not open DPFPDD." << endl;
        return;
    }

    DPFPDD_DEV_CAPS fingerprintDeviceCapabilities;
    fingerprintDeviceCapabilities.size = 100;
    rc = dpfpdd_get_device_capabilities(fingerprintDeviceHandle, &fingerprintDeviceCapabilities);
    if (rc != DPFPDD_SUCCESS)
    {
        cerr << "Could not get fingerprint device capabilities." << endl;
        return;
    }
    fingerprintDeviceImageRes = fingerprintDeviceCapabilities.resolutions[0];

    cout << "Set fingerprint device." << endl;
}

FMD captureAndConvertFingerprint()
{
    DPFPDD_DEV_STATUS fingerprintDeviceStatus;
    rc = dpfpdd_get_device_status(fingerprintDeviceHandle, &fingerprintDeviceStatus);
    if (rc != DPFPDD_SUCCESS)
    {
        cerr << "Fingerprint device is unavailable." << endl;
        return FMD();
    }

    DPFPDD_CAPTURE_PARAM captureParam = {0};
    captureParam.size = sizeof(captureParam);
    captureParam.image_fmt = FIDFormat;
    captureParam.image_proc = FIDProcessing;
    captureParam.image_res = fingerprintDeviceImageRes;

    DPFPDD_CAPTURE_RESULT captureResult = {0};
    captureResult.size = sizeof(captureResult);
    captureResult.info.size = sizeof(captureResult.info);

    FID fid;
    rc = dpfpdd_capture(fingerprintDeviceHandle, &captureParam, (unsigned int)(-1), &captureResult, &fid.size, fid.data);
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

// Database
sqlite3 *db;
string dbINIFilename = "../config/database.ini";
string dbSaveFilePath = "../data/database/";

struct Person
{
    string email;
    string firstName;
    string otherName;
    string lastName;
    bool isValid;
};

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

bool validateInput(string input, string datatype)
{
    bool isValid;
    if (datatype == "TEXT")
        isValid = regex_match(input, regex(R"([a-zA-Z]{2,})"));

    else if (datatype == "EMAIL")
        isValid = regex_match(input, regex(R"([a-zA-Z0-9._%+-]+@oauife\.edu\.ng)"));

    return isValid;
}

Person validatePersonInput(Person person = Person())
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

void insertPerson(Person personToInsert)
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

    cout << "Inserted person." << endl;
}

void insertFingerprint(string emailToInsert)
{
    FMD fmd = captureAndConvertFingerprint();

    if (!fmd.isEmpty)
    {
        const char *insertFingerprintSQL = "UPDATE Person SET fingerprint_data = ? WHERE email = ?";
        sqlite3_stmt *insertFingerprintStmt;

        if (sqlite3_prepare_v2(db, insertFingerprintSQL, -1, &insertFingerprintStmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_blob(insertFingerprintStmt, 1, fmd.data, fmd.size, SQLITE_STATIC);
            sqlite3_bind_text(insertFingerprintStmt, 2, emailToInsert.c_str(), -1, SQLITE_STATIC);
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

void enrolPerson(Person personToEnrol)
{
    Person validatedPersonToEnrol = validatePersonInput(personToEnrol);
    if (validatedPersonToEnrol.isValid)
    {
        insertPerson(validatedPersonToEnrol);
    }
}

void runFunctionInThread(function<void()> threadFunction)
{
    thread threadObject([threadFunction]()
                        { threadFunction(); });
    threadObject.detach();
}

void runFunctionInThread(function<void(Person)> threadFunction, Person person)
{
    thread threadObject([threadFunction, person]()
                        { threadFunction(person); });
    threadObject.detach();
}

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
    runFunctionInThread(setFingerprintDevice);

    // Create application window
    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr};
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
            ImGui::BeginChild("Enrol", ImVec2(1000, 0), true);
            {
                static char firstNameInput[256] = "";
                ImGui::InputText("First Name", firstNameInput, IM_ARRAYSIZE(firstNameInput));

                static char otherNameInput[256] = "";
                ImGui::InputText("Other Name", otherNameInput, IM_ARRAYSIZE(otherNameInput));

                static char lastNameInput[256] = "";
                ImGui::InputText("Last Name", lastNameInput, IM_ARRAYSIZE(lastNameInput));

                static char emailInput[256] = "";
                ImGui::InputText("Email", emailInput, IM_ARRAYSIZE(emailInput));

                if (ImGui::Button("Submit"))
                {
                    Person personToEnrol;
                    personToEnrol.email = string(emailInput);
                    personToEnrol.firstName = string(firstNameInput);
                    personToEnrol.otherName = string(otherNameInput);
                    personToEnrol.lastName = string(lastNameInput);

                    runFunctionInThread(enrolPerson, personToEnrol);
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