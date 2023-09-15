#include <iostream>
#include <string>
#include <conio.h>
#include <fstream>
#include "dpfpdd.h"
#include "dpfj.h"

using namespace std;

int main()
{
    ofstream outputFile;
    outputFile.open("output.txt", ios::out);

    if (!outputFile.is_open())
    {
        std::cerr << "Error: Could not open the file for writing." << std::endl;
        return 1; // Return an error code
    }

    char myTxt[] = "Testing...";
    outputFile << myTxt;
    outputFile.close();
}
