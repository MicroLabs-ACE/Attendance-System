#include <iostream>
#include <string>
#include <conio.h>
#include <fstream>
#include "dpfpdd.h"
#include "dpfj.h"

using namespace std;

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

int main()
{
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

        // Allocate memory for image_data, you may need to adjust the size
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

        // Save the fingerprint image data to a file
        ofstream outputFile("fingerprint_image.raw", ios::binary);
        if (outputFile.is_open())
        {
            outputFile.write(reinterpret_cast<const char *>(image_data), image_size);
            outputFile.close();
            cout << "Fingerprint image data saved to fingerprint_image.raw" << endl;
        }
        else
        {
            cout << "Error: Could not open the output file for writing." << endl;
        }

        // Free allocated memory for image_data
        free(image_data);
    }

    dpfpdd_exit();

    _getch();

    return 0;
}
