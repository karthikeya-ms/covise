/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include "WINSENTWindFarm.h"

#include <config/CoviseConfig.h>

#include <cover/coVRPluginSupport.h>

#include <osg/ComputeBoundsVisitor>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/Shape>
#include <osg/ShapeDrawable>
#include <osg/Vec3>

#include <osgDB/ReadFile>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>

using namespace covise;
using namespace opencover;

namespace
{
const char *s_configRoot = "COVER.Plugin.WINSENTWindFarm";

std::string cfgString(const std::string &entry, const std::string &defaultValue)
{
    return coCoviseConfig::getEntry("value", std::string(s_configRoot) + "." + entry, defaultValue);
}

bool parseCsvXYZLine(const std::string &line, osg::Vec3d &point)
{
    if (line.empty())
        return false;

    const char *cursor = line.c_str();
    char *end = nullptr;

    const double x = std::strtod(cursor, &end);
    if (end == cursor || *end != ',')
        return false;

    cursor = end + 1;
    const double y = std::strtod(cursor, &end);
    if (end == cursor || *end != ',')
        return false;

    cursor = end + 1;
    const double z = std::strtod(cursor, &end);
    if (end == cursor)
        return false;

    point.set(x, y, z);
    return true;
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

    root_ = new osg::MatrixTransform();
    root_->setName("WINSENTWindFarmRoot");

    const bool terrainOk = loadTerrainLayer(dataPath);
    const bool mastOk = loadMastLayer(dataPath);

    cover->getObjectsRoot()->addChild(root_.get());

    if (!terrainOk)
    {
        std::cerr << "WINSENTWindFarm: terrain not loaded. Check config at " << s_configRoot << std::endl;
        return false;
    }
    if (!mastOk)
    {
        std::cerr << "WINSENTWindFarm: mast layer not loaded. Check config at " << s_configRoot << std::endl;
    }

    return true;
}

bool WINSENT::loadTerrainLayer(const std::string &dataPath)
{
    siteRoot_ = new osg::MatrixTransform();
    siteRoot_->setName("WINSENTSite");
    siteRoot_->setMatrix(osg::Matrixd::identity());
    root_->addChild(siteRoot_.get());

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

    terrainNode->setName("WINSENTTerrain");
    siteRoot_->addChild(terrainNode.get());

    osg::ComputeBoundsVisitor cbv;
    terrainNode->accept(cbv);
    const osg::BoundingBoxd bb = cbv.getBoundingBox();
    if (bb.valid())
    {
        terrainMin_.set(bb.xMin(), bb.yMin(), bb.zMin());
        terrainMax_.set(bb.xMax(), bb.yMax(), bb.zMax());
        terrainBoundsValid_ = true;
    }

    return true;
}

bool WINSENT::loadMastLayer(const std::string &dataPath)
{
    if (!siteRoot_.valid())
        return false;

    const std::string mastBaseFile = cfgString("MastBaseFile", dataPath + "/MetMastBaseXYZ.csv");
    const std::string topologyFile = cfgString("TopologyFile", dataPath + "/Topology.csv");
    constexpr float mastHeight = 100.0f;
    constexpr float mastWidth = 2.0f;

    std::vector<osg::Vec3d> mastBases;
    if (!readCsvXYZ(mastBaseFile, mastBases))
    {
        std::cerr << "WINSENTWindFarm: failed to read mast base csv: " << mastBaseFile << std::endl;
        return false;
    }
    if (mastBases.empty())
    {
        std::cerr << "WINSENTWindFarm: no mast bases in csv: " << mastBaseFile << std::endl;
        return false;
    }

    osg::Vec3d topoMin;
    osg::Vec3d topoMax;
    if (!readCsvBoundsXYZ(topologyFile, topoMin, topoMax))
    {
        std::cerr << "WINSENTWindFarm: failed to read topology bounds from: " << topologyFile << std::endl;
        return false;
    }

    const double alignOffsetX = terrainBoundsValid_ ? (terrainMin_.x() - topoMin.x()) : 0.0;
    const double alignOffsetNorthing = terrainBoundsValid_ ? (terrainMin_.y() - topoMin.y()) : 0.0;
    const double alignOffsetElevation = terrainBoundsValid_ ? (terrainMin_.z() - topoMin.z()) : 0.0;

    const double offsetX = alignOffsetX;
    const double offsetNorthing = alignOffsetNorthing;
    const double offsetElevation = alignOffsetElevation;

    const osg::Vec4 mastColor(0.8f, 0.8f, 0.82f, 1.0f);

    osg::ref_ptr<osg::Group> mastGroup = new osg::Group();
    mastGroup->setName("WINSENTMasts");
    osg::ref_ptr<osg::Geode> mastGeode = new osg::Geode();
    mastGeode->setName("WINSENTMastGeode");

    for (const osg::Vec3d &base : mastBases)
    {
        const double alignedX = base.x() + offsetX;                     // east
        const double alignedNorthing = base.y() + offsetNorthing;       // north
        const double alignedElevation = base.z() + offsetElevation;      // up

        const osg::Vec3d localBase(alignedX, alignedNorthing, alignedElevation);
        osg::Vec3 center(static_cast<float>(localBase.x()),
                         static_cast<float>(localBase.y()),
                         static_cast<float>(localBase.z() + mastHeight * 0.5));
        osg::ref_ptr<osg::Box> mastBox = new osg::Box(center, mastWidth, mastWidth, mastHeight);

        osg::ref_ptr<osg::ShapeDrawable> mastDrawable = new osg::ShapeDrawable(mastBox.get());
        mastDrawable->setColor(mastColor);
        mastGeode->addDrawable(mastDrawable.get());
    }

    mastGroup->addChild(mastGeode.get());
    siteRoot_->addChild(mastGroup.get());

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

bool WINSENT::readCsvXYZ(const std::string &csvFile, std::vector<osg::Vec3d> &points)
{
    points.clear();

    std::ifstream in(csvFile);
    if (!in.is_open())
        return false;

    std::string line;
    while (std::getline(in, line))
    {
        osg::Vec3d point;
        if (!parseCsvXYZLine(line, point))
            continue;
        points.push_back(point);
    }

    return true;
}

bool WINSENT::readCsvBoundsXYZ(const std::string &csvFile, osg::Vec3d &minPoint, osg::Vec3d &maxPoint)
{
    std::ifstream in(csvFile);
    if (!in.is_open())
        return false;

    bool havePoint = false;
    minPoint.set(std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
    maxPoint.set(-std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(), -std::numeric_limits<double>::max());

    std::string line;
    while (std::getline(in, line))
    {
        osg::Vec3d point;
        if (!parseCsvXYZLine(line, point))
            continue;

        havePoint = true;
        minPoint.x() = std::min(minPoint.x(), point.x());
        minPoint.y() = std::min(minPoint.y(), point.y());
        minPoint.z() = std::min(minPoint.z(), point.z());
        maxPoint.x() = std::max(maxPoint.x(), point.x());
        maxPoint.y() = std::max(maxPoint.y(), point.y());
        maxPoint.z() = std::max(maxPoint.z(), point.z());
    }

    return havePoint;
}

COVERPLUGIN(WINSENT)
