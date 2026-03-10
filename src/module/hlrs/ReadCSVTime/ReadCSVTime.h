/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#ifndef _READ_CSV_TIME_H
#define _READ_CSV_TIME_H
// +++++++++++++++++++++++++++++++++++++++++
// MODULE ReadCSVTime
//
// This module reads time-dependent CSV formated ASCII files
//
#include <util/coviseCompat.h>
#include <api/coModule.h>
#include "reader/coReader.h"
#include "reader/ReaderControl.h"
using namespace covise;

enum
{
    TXT_BROWSER,
    MESHPORT3D,
    X_COORD,
    DPORT1_3D,
    DPORT2_3D,
    DPORT3_3D,
    DPORT4_3D,
    DPORT5_3D,
};
class ReadCSVTime : public coReader
{
public:
    typedef struct
    {
        float x, y, z;
    } Vect3;

    typedef struct
    {
        int assoc;
        int col;
        std::string name;
        coDistributedObject **dataObjs;
        std::string objectName;
        float *x_d;

    } VarInfo;

private:
    //  member functions
    virtual int compute(const char *port);
    virtual void param(const char *paraName, bool inMapLoading);

    // ports

    // ------------------------------------------------------------
    // -- parameters
    coChoiceParam *x_col, *y_col, *z_col, *time_col, *ID_col;
    coFloatParam *interval_size;
    coChoiceParam *p_dateFormat;
    bool is_dir = false;

    // utility functions
    int readHeader();
    int readASCIIData(const std::string &filePath);
    bool isDirectory(const std::string &path) const;
    std::string getFirstFileInDirectory(const std::string &dirPath);
    std::string getNthFileFromDirectory(const std::string &dirPath, size_t n);
    int readDirectory(const char *dirName);

    // this function can be combined with readAsCIIData, but for better readability and to avoid different behavior while changing reading files from directory, it is separated
    int ReadASCIIDataInDirectory(const std::string &filePath,
        std::vector<float> &allXData,
        std::vector<float> &allYData,
        std::vector<float> &allZData);
    bool isBiggerThanTimeInterval(char time_str[50]);

    float *xPtr = nullptr;
    float *yPtr = nullptr;
    float *zPtr = nullptr;  
    int addDataToGridPort(std::vector<float> &xData, std::vector<float> &yData, std::vector<float> &zData);

    // already opened file, always rewound after use
    FILE *d_dataFile;
    std::string fileName;

    void nextLine(FILE *fp);

    char buf[5000];
    int numLines;
    int numRows;

    std::vector<VarInfo> varInfos;

    int *v_l_tri;
    int *v_l_quad;
    float *xCoords;
    float *yCoords;
    float *zCoords;

    std::vector<float> id;
    int has_timestamps;

    std::vector<int> global_timeIntIdx;
    std::vector<int> global_timeInt;
    std::vector<int> global_NumOfVal;
    time_t global_last_t = 0;
    float global_last_millisec = 0.0f;

public:
    ReadCSVTime(int argc, char *argv[]);
    virtual ~ReadCSVTime();
};
#endif
