// #include <iostream>
// #include <string>
// #include <conio.h>
// #include <fstream>
// #include "dpfpdd.h"

// using namespace std;

// // Function to handle and print DP Capture API errors
// void handleDPFPDDError(DPFPDD_STATUS dpStatus, string successMessage)
// {
//     if (dpStatus == DPFPDD_SUCCESS)
//         cout << successMessage << endl;

//     else
//     {
//         cerr << "Error: " << dpStatus << endl;
//         dpfpdd_exit();
//         exit(1); // Exit the program with an error status
//     }
// }

// int main()
// {
//     DPFPDD_STATUS dpStatus;
//     DPFPDD_DEV_INFO devInfoArray[10];
//     unsigned int devCount = sizeof(devInfoArray) / sizeof(devInfoArray[0]);
//     DPFPDD_DEV devHandle = NULL;

//     dpStatus = dpfpdd_init();
//     handleDPFPDDError(dpStatus, "Initialized!");

//     dpStatus = dpfpdd_query_devices(&devCount, devInfoArray);
//     if (dpStatus != DPFPDD_SUCCESS)
//     {
//         handleDPFPDDError(dpStatus);
//         _getch();
//         return 1;
//     }
    

//     if (devCount == 0)
//     {
//         cerr << "No devices found." << endl;
//         dpfpdd_exit();
//         exit(1); // Exit the program with an error status
//     }

//     cout << "Found " << devCount << " device(s):" << endl;

//     for (unsigned int i = 0; i < devCount; ++i)
//     {
//         cout << "Device " << i + 1 << " Name: " << devInfoArray[i].name << endl;
//     }

//     // Reader information
//     char *readerName = devInfoArray[0].name;
//     DPFPDD_DEV readerHandle;

//     // Open the fingerprint reader
//     int openResult = dpfpdd_open(readerName, &readerHandle);

//     DPFPDD_DEV_CAPS devCaps;
//     cout << "Resolution: " << devCaps.size << endl;

//     int capResult = dpfpdd_get_device_capabilities(&readerHandle, &devCaps);


//     cout << "Result: " << capResult << endl;
//     cout << "Resolution: " << devCaps.size << endl;

//     dpStatus = dpfpdd_exit();
//     handleDPFPDDError(dpStatus, "Exited!");

//     _getch();
//     return 0;
// }
