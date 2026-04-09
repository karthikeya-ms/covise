/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#ifndef FOLLOW_OBJECT_H
#define FOLLOW_OBJECT_H

#include <api/coModule.h>
#include <do/coDoPoints.h>
#include <util/coviseCompat.h>

#include <string>

using namespace covise;

class FollowObject : public coModule
{
    COMODULE

public:
    FollowObject(int argc, char **argv);
    int compute(const char *port) override;

private:
    coInputPort *m_pathIn = nullptr;
    coOutputPort *m_pathOut = nullptr;

    coFileBrowserParam *m_modelFile = nullptr;
    coFloatParam *m_scale = nullptr;
    coFloatVectorParam *m_offset = nullptr;
    coBooleanParam *m_followHeading = nullptr;

    coDistributedObject *copyPathObject(const coDistributedObject *input, const coObjInfo &info);
    coDoPoints *copyPoints(const coDoPoints *points, const coObjInfo &info) const;
    void addFollowAttributes(coDistributedObject *obj) const;
    std::string formatScale() const;
    std::string formatOffset() const;
};

#endif
