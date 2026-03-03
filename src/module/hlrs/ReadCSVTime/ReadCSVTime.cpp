/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

/******************************************************************
 *
 *    ReadCSVTime
 *
 *
 *  Description: Read CSV files containing timesteps/ timestamps
 *  Date: 02.06.19
 *  Author: Leyla Kern
 *
 *******************************************************************/

#include "ReadCSVTime.h"

#ifndef _WIN32
#include <inttypes.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <string>

#include <errno.h>

#include <do/coDoData.h>
#include <do/coDoSet.h>
#include <do/coDoUnstructuredGrid.h>
#include <do/coDoPoints.h>

// add the following for directory reading
#include <util/coFileUtil.h>

// Convert 3-character month abbreviation to month number (0-11)
inline int monthNameToNumber(const char *month_str)
{
    const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    for (int i = 0; i < 12; i++)
    {
        if (strcasecmp(month_str, months[i]) == 0)
            return i;
    }
    return 0; // Default to January if not found
}

// Module set-up in Constructor
ReadCSVTime::ReadCSVTime(int argc, char *argv[])
    : coReader(argc, argv, "Read CSV data with timestamps")
{
    x_col = addChoiceParam("x_col", "Select column for x-coordinates");
    y_col = addChoiceParam("y_col", "Select column for y-coordinates");
    z_col = addChoiceParam("z_col", "Select column for z-coordinates");
    ID_col = addChoiceParam("ID", "Select column for ID");
    time_col = addChoiceParam("timestamp", "Select column for timestamp");
    interval_size = addFloatParam("time_interval", "Interval length in seconds");
    interval_size->setValue(1.0f);
    d_dataFile = NULL;

    const char *dFormatChoice[] = { "2019-01-01T08:15:00", "1/1/2019 8:15", "01.01.2019 08:15:00", "2019-01-01", "01-Jan 08:15:00.000" };
    p_dateFormat = addChoiceParam("DateFormat", "Select format of datetime");
    int numDataFormats = sizeof(dFormatChoice) / sizeof(dFormatChoice[0]);
    p_dateFormat->setValue(numDataFormats, dFormatChoice, 0);
}

ReadCSVTime::~ReadCSVTime()
{
}

// param callback read header again after all changes
void ReadCSVTime::param(const char *paramName, bool inMapLoading)
{

    FileItem *fii = READER_CONTROL->getFileItem(TXT_BROWSER);

    string txtBrowserName;
    if (fii)
    {
        txtBrowserName = fii->getName();
    }

    /////////////////  CALLED BY FILE BROWSER  //////////////////
    if (txtBrowserName == string(paramName))
    {
        FileItem *fi(READER_CONTROL->getFileItem(string(paramName)));
        if (fi)
        {
            coFileBrowserParam *bP = fi->getBrowserPtr();

            if (bP)
            {
                string dataNm(bP->getValue());
                if (dataNm.empty())
                {
                    cerr << "ReadCSVTime::param(..) no data file found " << endl;
                }
                else
                {
                    if (d_dataFile != NULL)
                        fclose(d_dataFile);
                    fileName = dataNm;
                    std::cout << "ReadCSVTime::param(..) selected file: " << fileName << std::endl;
                    int result = STOP_PIPELINE;
                    is_dir = isDirectory(dataNm);
                    std::cout << "Read directory: " << is_dir << std::endl;
                    if (!is_dir)
                    {
                        d_dataFile = fopen(dataNm.c_str(), "r");
                        if (d_dataFile != NULL)
                        {
                            result = readHeader();
                            fseek(d_dataFile, 0, SEEK_SET);
                        }
                        else
                        {
                            cerr << "ReadCSVTime::param(..) could not open file: " << dataNm.c_str() << endl;
                        }
                    }
                    else if (is_dir)
                    {
                        const char *dirName = coDirectory::dirOf(dataNm.c_str());
                        if (dirName != NULL)
                        {
                            coDirectory *dir = coDirectory::open(dirName);
                            sendInfo("ReadCSVTime::param(..) opened directory: %s", dirName);
                            sendInfo("ReadCSVTime::param(..) found %d files in directory", dir->count());

                            std::string first_file = getNthFileFromDirectory(dataNm, 0);
                            sendInfo("ReadCSVTime::param(..) read header information from first file called %s", first_file.c_str());

                            if (!first_file.empty())
                            {
                                // read first file in directory to get header info
                                d_dataFile = fopen(first_file.c_str(), "r");
                                if (d_dataFile != NULL)
                                {
                                    result = readHeader();
                                    fseek(d_dataFile, 0, SEEK_SET);
                                }
                                else
                                {
                                    cerr << "ReadCSVTime::param(..) could not open file: " << first_file.c_str() << endl;
                                }
                            }
                            else
                            {
                                cerr << "ReadCSVTime::param(..) directory is empty: " << dirName << endl;
                            }
                        }
                        else
                        {
                            cerr << "ReadCSVTime::param(..) could not open directory: " << dirName << endl;
                        }
                    }

                    if (result == STOP_PIPELINE)
                    {
                        cerr << "ReadCSVTime::param(..) could notdir->name(0) read file: " << dataNm.c_str() << endl;
                    }
                    else
                    {

                        // lists for Choice Labels
                        vector<string> dataChoices;

                        // fill in NONE to READ no data
                        string noneStr("NONE");
                        dataChoices.push_back(noneStr);

                        // fill in all species for the appropriate Ports/Choices
                        for (int i = 0; i < varInfos.size(); i++)
                        {
                            dataChoices.push_back(varInfos[i].name);
                        }
                        if (inMapLoading)
                            return;
                        READER_CONTROL->updatePortChoice(DPORT1_3D, dataChoices);
                        READER_CONTROL->updatePortChoice(DPORT2_3D, dataChoices);
                        READER_CONTROL->updatePortChoice(DPORT3_3D, dataChoices);
                        READER_CONTROL->updatePortChoice(DPORT4_3D, dataChoices);
                        READER_CONTROL->updatePortChoice(DPORT5_3D, dataChoices);
                        if (varInfos.size() > 0)
                            x_col->setValue((int)varInfos.size() + 1, dataChoices, 1);
                        if (varInfos.size() > 1)
                            y_col->setValue((int)varInfos.size() + 1, dataChoices, 2);
                        if (varInfos.size() > 2)
                            z_col->setValue((int)varInfos.size() + 1, dataChoices, 3);
                        if (varInfos.size() > 1)
                        {
                            ID_col->setValue((int)varInfos.size() + 1, dataChoices, 1);
                            time_col->setValue((int)varInfos.size() + 1, dataChoices, 2);
                        }
                        dataChoices.clear();

                        // const char *dFormatChoice[] = {"2019-01-01T08:15:00","1/1/2019 8:15","01.01.2019 08:15:00","2019-01-01"};
                        // p_dateFormat->setValue(sizeof(dFormatChoice)/sizeof(dFormatChoice[0]), dFormatChoice, 0);
                    }
                }
                return;
            }

            else
            {
                cerr << "ReadCSVTime::param(..) BrowserPointer NULL " << endl;
            }
        }
    }
}

void ReadCSVTime::nextLine(FILE *fp)
{
    if (fgets(buf, sizeof(buf), fp) == NULL)
    {
        cerr << "ReadCSVTime::nextLine: fgets failed" << endl;
    }
}

int ReadCSVTime::readHeader()
{

    int ii_choiseVals = 0;

    nextLine(d_dataFile);
    sendInfo("Found header line : %s", buf);

    buf[strlen(buf) - 1] = '\0';

    std::vector<std::string> names;
    varInfos.clear();
    names.clear();
    const char *name = strtok(buf, ";,");
    names.push_back(name);

    while ((name = strtok(NULL, ";,")) != NULL)
    {
        names.push_back(name);
    }

    if (names.size() > 0)
    {

        for (int i = 0; i < names.size(); i++)
        {
            VarInfo vi;
            vi.name = names[i];
            varInfos.push_back(vi);
        }

        sendInfo("Found %lu header elements", (unsigned long)names.size());

        numRows = 0;
        while (fgets(buf, sizeof(buf), d_dataFile) != NULL)
        {
            numRows = numRows + 1;
        }
        coModule::sendInfo("Found %d data lines", numRows);

        rewind(d_dataFile);
        nextLine(d_dataFile);

        return CONTINUE_PIPELINE;
    }
    else
    {
        return STOP_PIPELINE;
    }
}

static bool isCSVFile(const std::string &filename)
{
    if (filename.length() < 4)
        return false;
    std::string ext = filename.substr(filename.length() - 4);
    return ext == ".csv" || ext == ".CSV";
}

int ReadCSVTime::readDirectory(const char *dirName)
{
    coDirectory *dir = coDirectory::open(dirName);

    for (int i = 0; i < dir->count(); i++)
    {
        std::string fileStr = getNthFileFromDirectory(dirName, i);

        if (!isCSVFile(fileStr))
            continue; // Skip non-CSV files

        try
        {
            std::cout << "Reading file in readDirectory: " << fileStr << std::endl;
            readASCIIData(fileStr);
        }
        catch (...)
        {
            cerr << "ReadCSVTime::readDirectory(..) could not read file: " << fileStr << endl;
        }
    }
    return CONTINUE_PIPELINE;
}

// taken from old ReadCSVTime module: 2-Pass reading
int ReadCSVTime::readASCIIData(const std::string &filePath)
{
    FILE *dataFile = fopen(filePath.c_str(), "r");
    if (!dataFile)
    {
        cerr << "ReadCSVTime::readASCIIData(..) could not open file: " << filePath << endl;
        return STOP_PIPELINE;
    }

    char *cbuf;
    int ii, RowCount;
    int CurrRow;
    std::vector<float> tmpdat(varInfos.size());
    std::string name_extension = "";

    if (varInfos.size() > 0) // Check that header was already read
    {
        int col_for_x = x_col->getValue() - 1;
        int col_for_y = y_col->getValue() - 1;
        int col_for_z = z_col->getValue() - 1;
        int col_for_id = ID_col->getValue() - 1;
        int col_for_time = time_col->getValue() - 1;
        float MAX_TIME_FLOAT = interval_size->getValue();
        int dFormat = p_dateFormat->getValue();

        printf("%d %d %d\n", col_for_x, col_for_y, col_for_z);

        if (col_for_x == -1)
            coModule::sendWarning("No column selected for x-coordinates");
        if (col_for_y == -1)
            coModule::sendWarning("No column selected for y-coordinates");
        if (col_for_z == -1)
            coModule::sendWarning("No column selected for z-coordinates");

        if (col_for_time >= 0)
        {
            has_timestamps = 1;
            name_extension = "_tmp";
        }
        if ((col_for_x >= 0) && (col_for_y >= 0) && (col_for_z >= 0))
        {
            std::string objNameBase = READER_CONTROL->getAssocObjName(MESHPORT3D);
            sprintf(buf, "%s%s", objNameBase.c_str(), name_extension.c_str());
            coDoPoints *grid = new coDoPoints(buf, numRows);

            grid->getAddresses(&xCoords, &yCoords, &zCoords);
        }

        for (int n = 0; n < varInfos.size(); n++)
        {
            varInfos[n].dataObjs = new coDistributedObject *[1];
            varInfos[n].dataObjs[0] = NULL;
            varInfos[n].assoc = 0;
        }
        int portID = 0;
        int number_of_ports = 5;
        for (int n = 0; n < number_of_ports; n++)
        {
            int pos = READER_CONTROL->getPortChoice(DPORT1_3D + n);
            // printf("%d %d\n",pos,n);
            if (pos > 0)
            {
                if (varInfos[pos - 1].assoc == 0)
                {
                    portID = DPORT1_3D + n;
                    sprintf(buf, "%s%s", READER_CONTROL->getAssocObjName(DPORT1_3D + n).c_str(), name_extension.c_str());
                    coDoFloat *dataObj = new coDoFloat(buf, numRows);
                    varInfos[pos - 1].dataObjs[0] = dataObj;
                    varInfos[pos - 1].assoc = 1;
                    dataObj->getAddress(&varInfos[pos - 1].x_d);
                }
                else
                {
                    sendWarning("Column %s already associated to port %d", varInfos[pos - 1].name.c_str(), n);
                }
            }
        }
        RowCount = 0;
        CurrRow = 0;

        int timeInt = 0;
        float *xValInt, *yValInt, *zValInt;
        std::vector<int> timeIntIdx;
        std::vector<int> NumOfVal;
        char time_str[50];
        struct tm tm = {};
        time_t last_t = 0;
        float last_millisec = 0.0f; // Store last milliseconds

        while (fgets(buf, sizeof(buf), dataFile) != NULL)
        {

            for (int i = 0; i < varInfos.size(); i++)
            {
                tmpdat[i] = 0.;
            }

            if ((cbuf = strtok(buf, ",;")) != NULL)
            {
                if (col_for_time == 0)
                {
                    sscanf(cbuf, "%[^\n]s", time_str);
                }
                sscanf(cbuf, "%f", &tmpdat[0]);
            }
            else
            {
                coModule::sendWarning("Error parsing line %d", RowCount + 1);
            }

            ii = 0;
            while ((cbuf = strtok(NULL, ";,")) != NULL)
            {
                ii = ii + 1;
                sscanf(cbuf, "%f", &tmpdat[ii]);

                if (ii == col_for_time)
                    sscanf(cbuf, "%[^\n]s", time_str);
            }
            if (has_timestamps != 0)
            {
                float millisec = 0.0f; // Store milliseconds for this row

                if (dFormat == 0)
                {
                    // strptime(time_str, "%Y-%m-%dT%H:%M:%S", &tm);
                    sscanf(time_str, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
                }
                else if (dFormat == 1)
                {
                    // strptime(time_str, "%d/%m/%Y %H:%M", &tm);
                    sscanf(time_str, "%d/%d/%d %d:%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year, &tm.tm_hour, &tm.tm_min);
                }
                else if (dFormat == 2)
                {
                    // strptime(time_str, "%Y.%m.%dT%H:%M", &tm);
                    sscanf(time_str, "%d.%d.%dT%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min);
                }
                else if (dFormat == 3)
                {
                    // strptime(time_str, "%Y-%m-%d",&tm);
                    sscanf(time_str, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
                }
                else if (dFormat == 4)
                {
                    char month_str[4] = "";
                    sscanf(time_str, "%d-%3s %d:%d:%d.%f", &tm.tm_mday, month_str, &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &millisec);
                    tm.tm_mon = monthNameToNumber(month_str);
                    // year is not given in this format -> set to 2025
                    tm.tm_year = 125; // 2025 - 1900
                }
                time_t t = mktime(&tm);

                // Calculate time difference including milliseconds
                double time_diff = difftime(t, last_t);

                const double EPSILON = 1e-9;

                if (fabs(time_diff) < EPSILON)
                {
                    time_diff = millisec / 1000.0f - last_millisec / 1000.0f;
                }

                if ((time_diff > (MAX_TIME_FLOAT)) || CurrRow == 0)
                {
                    timeIntIdx.push_back(CurrRow);
                    timeInt++;
                    for (int i = 0; i < varInfos.size(); i++)
                    {
                        if (varInfos[i].assoc == 1)
                        {
                            varInfos[i].x_d[CurrRow] = tmpdat[i];
                        }
                    }
                    if ((col_for_x >= 0) && (col_for_y >= 0) && (col_for_z >= 0))
                    {
                        xCoords[CurrRow] = tmpdat[col_for_x];
                        yCoords[CurrRow] = tmpdat[col_for_y];
                        zCoords[CurrRow] = tmpdat[col_for_z];
                    }
                    id.push_back(static_cast<int>(tmpdat[col_for_id]));
                    NumOfVal.push_back(1);
                    CurrRow++;
                }
                else // check if sensor *id* occurs multiple times in interval
                {
                    auto idx_f = CurrRow + 10;
                    int tmp_idx = 1;
                    if (timeIntIdx.size() >= 1)
                    {
                        tmp_idx = timeIntIdx[timeIntIdx.size() - 1];
                        auto idx_ptr = std::find(id.begin() + tmp_idx, id.begin() + CurrRow, static_cast<int>(tmpdat[col_for_id]));
                        idx_f = std::distance(id.begin() /*+ tmp_idx*/, idx_ptr);
                    }
                    else if (id[0] == static_cast<int>(tmpdat[col_for_id]))
                    {
                        idx_f = 0;
                    }
                    if (idx_f < CurrRow) // else if index point to last element -> nothing was found
                    { // double occurence of ID
                        for (int i = 0; i < varInfos.size(); i++)
                        {
                            if (varInfos[i].assoc == 1)
                            {
                                varInfos[i].x_d[idx_f] += tmpdat[i];
                            }
                        }
                        NumOfVal[idx_f] += 1;
                    }
                    else
                    { // ID does not occure in current interval -> add to interval
                        for (int i = 0; i < varInfos.size(); i++)
                        {
                            if (varInfos[i].assoc == 1)
                            {
                                varInfos[i].x_d[CurrRow] = tmpdat[i];
                            }
                        }
                        if ((col_for_x >= 0) && (col_for_y >= 0) && (col_for_z >= 0))
                        {
                            xCoords[CurrRow] = tmpdat[col_for_x];
                            yCoords[CurrRow] = tmpdat[col_for_y];
                            zCoords[CurrRow] = tmpdat[col_for_z];
                        }
                        id.push_back(static_cast<int>(tmpdat[col_for_id]));
                        NumOfVal.push_back(1);
                        CurrRow++;
                    }
                }

                // Always update last_t and last_millisec for every row
                last_t = t;
                last_millisec = millisec;
            }
            else
            {
                for (int i = 0; i < varInfos.size(); i++)
                {
                    if (varInfos[i].assoc == 1)
                    {
                        varInfos[i].x_d[RowCount] = tmpdat[i];
                    }
                }
                if ((col_for_x >= 0) && (col_for_y >= 0) && (col_for_z >= 0))
                {
                    xCoords[RowCount] = tmpdat[col_for_x];
                    yCoords[RowCount] = tmpdat[col_for_y];
                    zCoords[RowCount] = tmpdat[col_for_z];
                }
            }

            RowCount = RowCount + 1;
        }

        timeIntIdx.push_back(CurrRow - 1);
        if (has_timestamps != 0)
        {
            for (int j = 0; j < NumOfVal.size(); j++)
            {
                if (NumOfVal[j] > 1)
                {
                    for (int i = 0; i < varInfos.size(); i++)
                    {
                        if (varInfos[i].assoc == 1)
                        {
                            varInfos[i].x_d[j] = varInfos[i].x_d[j] / NumOfVal[j];
                        }
                    }
                }
            }
            sendInfo("Found %d time intervals", timeInt);

            // Use vector instead of raw pointer array
            std::vector<coDistributedObject *> time_outdat(timeInt + 1, nullptr);
            std::vector<coDistributedObject *> time_outdat_grid(timeInt + 1, nullptr);

            for (int n = 0; n < 5; n++)
            {
                portID = DPORT1_3D + n;
                int pos = READER_CONTROL->getPortChoice(DPORT1_3D + n);
                if (pos > 0)
                {

                    int idx, idx1, numValuesInInt, t;
                    for (t = 0; t < (timeInt); t++)
                    {
                        idx = timeIntIdx[t];
                        idx1 = timeIntIdx[t + 1];
                        numValuesInInt = idx1 - idx + 1;
                        float *val;
                        sprintf(buf, "%s_%d", READER_CONTROL->getAssocObjName(DPORT1_3D + n).c_str(), t);
                        coDoFloat *p = new coDoFloat(buf, numValuesInInt);
                        p->getAddress(&val);
                        time_outdat[t] = p;
                        for (int j = 0; j < numValuesInInt; j++)
                        {
                            val[j] = varInfos[pos - 1].x_d[idx + j];
                        }
                    }
                    time_outdat[timeInt] = NULL;
                    coDoSet *outdata = new coDoSet(READER_CONTROL->getAssocObjName(portID).c_str(), time_outdat.data());
                    varInfos[pos - 1].dataObjs[0] = outdata;
                    sprintf(buf, "1 %d", timeInt);
                    outdata->addAttribute("TIMESTEP", buf);
                }
            }
            int idx, idx1, numValuesInInt, t;
            std::string objNameBase = READER_CONTROL->getAssocObjName(MESHPORT3D);
            for (t = 0; t < (timeInt); t++)
            {
                idx = timeIntIdx[t];
                idx1 = timeIntIdx[t + 1];
                numValuesInInt = idx1 - idx + 1;
                sprintf(buf, "%s_%d", objNameBase.c_str(), t);
                coDoPoints *gridInt = new coDoPoints(buf, numValuesInInt);
                time_outdat_grid[t] = gridInt;
                gridInt->getAddresses(&xValInt, &yValInt, &zValInt);
                for (int j = 0; j < numValuesInInt; j++)
                {
                    xValInt[j] = xCoords[idx + j];
                    yValInt[j] = yCoords[idx + j];
                    zValInt[j] = zCoords[idx + j];
                }
            }
            time_outdat_grid[timeInt] = NULL;
            coDoSet *outdata_grid = new coDoSet(objNameBase.c_str(), time_outdat_grid.data());
            sprintf(buf, "1 %d", timeInt);
            outdata_grid->addAttribute("TIMESTEP", buf);
        }

        for (int n = 0; n < varInfos.size(); n++)
        {
            delete[] varInfos[n].dataObjs;
            varInfos[n].assoc = 0;
        }

        id.clear();
        has_timestamps = 0;

        fclose(dataFile); // Close at the end
        return CONTINUE_PIPELINE;
    }
}

int ReadCSVTime::compute(const char *port)
{
    int result = STOP_PIPELINE;
    has_timestamps = 0;
    if (fileName.empty())
    {
        cerr << "ReadCSVTime::param(..) no data file found " << endl;
        return result;
    }

    if (is_dir == 0)
    {
        result = readASCIIData(fileName);
    }
    else if (is_dir == 1)
    {
        sendInfo("ReadCSVTime::compute(..) opened directory: %s", fileName.c_str());
        result = readDirectory(fileName.c_str());
    }
    else
    {
        cerr << "ReadCSVTime::compute(..) could not open file: " << fileName << endl;
    }

    return result;
}

bool ReadCSVTime::isDirectory(const std::string &path) const
{
    if (path.empty())
        return true;

    return path.back() == '/';
}

std::string ReadCSVTime::getFirstFileInDirectory(const std::string &dirPath)
{
    coDirectory *dir = coDirectory::open(dirPath.c_str());
    if (!dir)
        return {};

    for (int i = 0; i < dir->count(); ++i)
    {
        const char *name = dir->name(i);
        if (!name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        std::string fullPath = dirPath;
        if (!fullPath.empty() && fullPath.back() != '/')
            fullPath += '/';
        fullPath += name;

        struct stat st {};
        if (stat(fullPath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            return fullPath;
    }
    return {};
}

std::string ReadCSVTime::getNthFileFromDirectory(const std::string &dirPath, size_t n)
{
    coDirectory *dir = coDirectory::open(dirPath.c_str());
    if (!dir)
        return {};

    std::vector<std::string> files;
    files.reserve(dir->count());

    for (int i = 0; i < dir->count(); ++i)
    {
        const char *name = dir->name(i);
        if (!name || std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0)
            continue;

        std::string fullPath = dirPath;
        if (!fullPath.empty() && fullPath.back() != '/')
            fullPath += '/';
        fullPath += name;

        struct stat st {};
        if (stat(fullPath.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            files.push_back(fullPath);
    }

    std::sort(files.begin(), files.end()); // deterministic order

    if (n >= files.size())
        return {};

    return files[n]; // n is 0-based
}

int main(int argc, char *argv[])
{

    // define outline of reader
    READER_CONTROL->addFile(TXT_BROWSER, "data_file_path", "Data file path", ".", "*.csv;*.CSV");

    READER_CONTROL->addOutputPort(MESHPORT3D, "geoOut_3D", "Points", "Geometry", false);

    READER_CONTROL->addOutputPort(DPORT1_3D, "data1_3D", "Float|Vec3", "data1-3d");
    READER_CONTROL->addOutputPort(DPORT2_3D, "data2_3D", "Float|Vec3", "data2-3d");
    READER_CONTROL->addOutputPort(DPORT3_3D, "data3_3D", "Float|Vec3", "data3-3d");
    READER_CONTROL->addOutputPort(DPORT4_3D, "data4_3D", "Float|Vec3", "data4-3d");
    READER_CONTROL->addOutputPort(DPORT5_3D, "data5_3D", "Float|Vec3", "data5-3d");

    // create the module
    coReader *application = new ReadCSVTime(argc, argv);

    // this call leaves with exit(), so we ...
    application->start(argc, argv);

    // ... never reach this point
    return 0;
}