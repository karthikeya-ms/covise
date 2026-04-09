/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include "FollowObject.h"

#include <do/coDoSet.h>

#include <cstdio>
#include <sstream>
#include <vector>

namespace
{
const char *PluginName = "FollowObject";
}

FollowObject::FollowObject(int argc, char **argv)
    : coModule(argc, argv, "Tag a trajectory so OpenCOVER can animate a single object with transforms")
{
    m_pathIn = addInputPort("pathIn", "Points", "trajectory points or timestep points");
    m_pathOut = addOutputPort("pathOut", "Points", "trajectory tagged for FollowObject OpenCOVER plugin");

    m_modelFile = addFileBrowserParam("model_file", "optional object file; leave empty to use the built-in placeholder UAV");
    m_modelFile->setValue("", "*");

    m_scale = addFloatParam("scale", "uniform scale applied in OpenCOVER");
    m_scale->setValue(1.0f);

    m_offset = addFloatVectorParam("offset", "local model offset applied before rotation and translation");
    m_offset->setValue(0.0f, 0.0f, 0.0f);

    m_followHeading = addBooleanParam("follow_heading", "rotate the object to face the direction of motion");
    m_followHeading->setValue(true);
}

int FollowObject::compute(const char *)
{
    const coDistributedObject *input = m_pathIn->getCurrentObject();
    if (!input)
    {
        sendError("No object connected at port '%s'", m_pathIn->getName());
        return STOP_PIPELINE;
    }

    coDistributedObject *output = copyPathObject(input, coObjInfo(m_pathOut->getObjName()));
    if (!output)
    {
        return STOP_PIPELINE;
    }

    addFollowAttributes(output);
    m_pathOut->setCurrentObject(output);
    return CONTINUE_PIPELINE;
}

coDistributedObject *FollowObject::copyPathObject(const coDistributedObject *input, const coObjInfo &info)
{
    if (const auto *set = dynamic_cast<const coDoSet *>(input))
    {
        int numElements = 0;
        const coDistributedObject *const *elements = set->getAllElements(&numElements);
        std::vector<const coDistributedObject *> copied;
        copied.reserve(numElements + 1);

        for (int i = 0; i < numElements; ++i)
        {
            char name[256];
            std::snprintf(name, sizeof(name), "%s_%d", info.getName(), i);
            coDistributedObject *child = copyPathObject(elements[i], coObjInfo(name));
            if (!child)
            {
                for (const auto *obj : copied)
                {
                    delete obj;
                }
                return nullptr;
            }
            copied.push_back(child);
        }
        copied.push_back(nullptr);

        auto *outSet = new coDoSet(info, copied.data());
        if (!outSet->objectOk())
        {
            sendError("Failed to create output set '%s'", info.getName());
            delete outSet;
            for (const auto *obj : copied)
            {
                delete obj;
            }
            return nullptr;
        }

        outSet->copyAllAttributes(input);
        return outSet;
    }

    if (const auto *points = dynamic_cast<const coDoPoints *>(input))
    {
        auto *outPoints = copyPoints(points, info);
        if (!outPoints || !outPoints->objectOk())
        {
            sendError("Failed to create output points '%s'", info.getName());
            delete outPoints;
            return nullptr;
        }
        outPoints->copyAllAttributes(input);
        return outPoints;
    }

    sendError("FollowObject only supports Points or timestep sets of Points");
    return nullptr;
}

coDoPoints *FollowObject::copyPoints(const coDoPoints *points, const coObjInfo &info) const
{
    float *x = nullptr;
    float *y = nullptr;
    float *z = nullptr;
    points->getAddresses(&x, &y, &z);
    return new coDoPoints(info, points->getNumPoints(), x, y, z);
}

void FollowObject::addFollowAttributes(coDistributedObject *obj) const
{
    obj->addAttribute("MODULE", PluginName);
    obj->addAttribute("FOLLOW_OBJECT", "1");
    obj->addAttribute("FOLLOW_MODEL", m_modelFile->getValue());
    obj->addAttribute("FOLLOW_SCALE", formatScale().c_str());
    obj->addAttribute("FOLLOW_OFFSET", formatOffset().c_str());
    obj->addAttribute("FOLLOW_HEADING", m_followHeading->getValue() ? "1" : "0");
}

std::string FollowObject::formatScale() const
{
    std::ostringstream out;
    out << m_scale->getValue();
    return out.str();
}

std::string FollowObject::formatOffset() const
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    m_offset->getValue(x, y, z);

    std::ostringstream out;
    out << x << ' ' << y << ' ' << z;
    return out.str();
}

MODULE_MAIN(IO, FollowObject)
