#include <iostream>
#include <string>
#include <conio.h>
#include <fstream>
#include "dpfpdd.h"

using namespace std;

// Function to handle and print DP Capture API errors
void handleDPFPDDError(DPFPDD_STATUS dpStatus, string successMessage)
{
    if (dpStatus == DPFPDD_SUCCESS)
        cout << successMessage << endl;

    else
    {
        cerr << "Error: " << dpStatus << endl;
        dpfpdd_exit();
        exit(1); // Exit the program with an error status
    }
}

int main()
{
    DPFPDD_STATUS dpStatus;
    DPFPDD_DEV_INFO devInfoArray[10];
    unsigned int devCount = sizeof(devInfoArray) / sizeof(devInfoArray[0]);
    DPFPDD_DEV devHandle = NULL;

    dpStatus = dpfpdd_init();
    handleDPFPDDError(dpStatus, "Initialized!");

    dpStatus = dpfpdd_query_devices(&devCount, devInfoArray);
    handleDPFPDDError(dpStatus, "Queried devices.");

    if (devCount == 0)
    {
        cerr << "No devices found." << endl;
        dpfpdd_exit();
        exit(1); // Exit the program with an error status
    }

    cout << "Found " << devCount << " device(s):" << endl;

    for (unsigned int i = 0; i < devCount; ++i)
        cout << "Device " << i + 1 << " Name: " << devInfoArray[i].name << endl;

    dpStatus = dpfpdd_exit();
    handleDPFPDDError(dpStatus, "Exited!");

    _getch();
    return 0;
}
