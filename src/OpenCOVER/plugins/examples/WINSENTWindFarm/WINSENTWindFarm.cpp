/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include "WINSENTWindFarm.h"

#include <config/CoviseConfig.h>

#include <cover/coVRPluginSupport.h>

#include <osg/Math>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/Vec3>

#include <osgDB/ReadFile>

#include <algorithm>
#include <cctype>
#include <iostream>

using namespace covise;
using namespace opencover;

namespace
{
const char *s_configRoot = "COVER.Plugin.WINSENTWindFarm";

std::string cfgString(const std::string &entry, const std::string &defaultValue)
{
    return coCoviseConfig::getEntry("value", std::string(s_configRoot) + "." + entry, defaultValue);
}

float cfgFloat(const std::string &entry, float defaultValue)
{
    return coCoviseConfig::getFloat("value", std::string(s_configRoot) + "." + entry, defaultValue);
}

bool cfgBool(const std::string &entry, bool defaultValue)
{
    std::string v = cfgString(entry, defaultValue ? "true" : "false");
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    return (v == "1" || v == "true" || v == "on" || v == "yes");
}

osg::Matrixd buildSceneTransform()
{
    const float scale = cfgFloat("SceneScale", 1.0f);
    const float tx = cfgFloat("ScenePosX", 0.0f);
    const float ty = cfgFloat("ScenePosY", 0.0f);
    const float tz = cfgFloat("ScenePosZ", 0.0f);
    const float rx = cfgFloat("SceneRotXDeg", 0.0f);
    const float ry = cfgFloat("SceneRotYDeg", 0.0f);
    const float rz = cfgFloat("SceneRotZDeg", 0.0f);

    return osg::Matrixd::scale(scale, scale, scale)
        * osg::Matrixd::rotate(osg::DegreesToRadians(rx), osg::Vec3(1.0f, 0.0f, 0.0f))
        * osg::Matrixd::rotate(osg::DegreesToRadians(ry), osg::Vec3(0.0f, 1.0f, 0.0f))
        * osg::Matrixd::rotate(osg::DegreesToRadians(rz), osg::Vec3(0.0f, 0.0f, 1.0f))
        * osg::Matrixd::translate(tx, ty, tz);
}

} // namespace

WINSENT::WINSENT()
    : coVRPlugin(COVER_PLUGIN_NAME)
{
}

WINSENT::~WINSENT()
{
    if (root_.valid() && cover && cover->getObjectsRoot())
        cover->getObjectsRoot()->removeChild(root_.get());
}

bool WINSENT::init()
{
    const std::string dataPath = cfgString("DataPath", "/home/hpcsmand/MultiSensorSWE1");
    const CoordinateMode terrainMode = readCoordinateMode(cfgString("CoordinateMode", "YUp"));
    const bool verbose = cfgBool("Verbose", true);

    root_ = new osg::MatrixTransform();
    root_->setName("WINSENTWindFarmRoot");
    root_->setMatrix(buildSceneTransform());

    const bool terrainOk = loadTerrainLayer(dataPath, terrainMode);

    if (verbose)
    {
        std::cerr << "WINSENTWindFarm: DataPath=" << dataPath
                  << " TerrainMode=" << (terrainMode == CoordinateMode::YUp ? "YUp" : "ZUp")
                  << std::endl;
    }

    cover->getObjectsRoot()->addChild(root_.get());

    if (!terrainOk)
    {
        std::cerr << "WINSENTWindFarm: terrain not loaded. Check config at " << s_configRoot << std::endl;
        return false;
    }

    return true;
}

void WINSENT::preFrame()
{
}

bool WINSENT::loadTerrainLayer(const std::string &dataPath, CoordinateMode mode)
{
    if (!cfgBool("TerrainEnabled", true))
        return true;

    std::string terrainModel = cfgString("TerrainModel", dataPath + "/winsent_terrain_yup.obj");
    osg::ref_ptr<osg::Node> terrainNode = loadTerrainModel(terrainModel);

    if (!terrainNode.valid())
    {
        // Fallback to .ive if .obj is not available.
        terrainModel = cfgString("TerrainModelFallback", dataPath + "/winsent_terrain_yup.ive");
        terrainNode = loadTerrainModel(terrainModel);
    }

    if (!terrainNode.valid())
    {
        std::cerr << "WINSENTWindFarm: failed to load terrain model."
                  << " Tried configured terrain files in " << dataPath << std::endl;
        return false;
    }

    if (mode == CoordinateMode::YUp)
    {
        osg::ref_ptr<osg::MatrixTransform> t = new osg::MatrixTransform();
        t->setName("WINSENTTerrain");
        t->setMatrix(osg::Matrixd::rotate(osg::DegreesToRadians(90.0), osg::Vec3(1.0, 0.0, 0.0)));
        t->addChild(terrainNode.get());
        root_->addChild(t.get());
    }
    else
    {
        terrainNode->setName("WINSENTTerrain");
        root_->addChild(terrainNode.get());
    }

    return true;
}

osg::ref_ptr<osg::Node> WINSENT::loadTerrainModel(const std::string &terrainModelFile) const
{
    osg::ref_ptr<osg::Node> node = osgDB::readRefNodeFile(terrainModelFile);
    if (!node.valid())
    {
        std::cerr << "WINSENTWindFarm: failed to load terrain model: " << terrainModelFile << std::endl;
    }
    return node;
}

WINSENT::CoordinateMode WINSENT::readCoordinateMode(const std::string &modeText)
{
    std::string mode = modeText;
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (mode == "yup" || mode == "y-up" || mode == "y_up")
        return CoordinateMode::YUp;

    return CoordinateMode::ZUp;
}

COVERPLUGIN(WINSENT)
