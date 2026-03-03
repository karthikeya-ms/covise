/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include "WINSENTWindFarm.h"

#include <config/CoviseConfig.h>

#include <cover/coVRPluginSupport.h>

#include <osg/ComputeBoundsVisitor>
#include <osg/Geode>
#include <osg/GL>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/Plane>
#include <osg/Shape>
#include <osg/ShapeDrawable>
#include <osg/StateSet>
#include <osg/TexGen>
#include <osg/TexEnv>
#include <osg/Texture2D>
#include <osg/Vec3>

#include <osgDB/ReadFile>

#include <algorithm>
#include <cctype>
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

bool endsWith(const std::string &value, const std::string &suffix)
{
    if (suffix.size() > value.size())
        return false;
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin(),
                      [](char a, char b) { return std::tolower(a) == std::tolower(b); });
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

    // Prefer textured OBJ terrain and map it into the same Z-up frame that the
    // workflows use, so launch behavior is stable with/without COCONFIG.
    const std::string terrainPreferred = cfgString("TerrainModel", dataPath + "/winsent_terrain_yup.obj");
    const std::string terrainAlternative = cfgString("TerrainModelFallback", dataPath + "/winsent_terrain_yup.ive");

    std::string terrainModel = terrainPreferred;
    osg::ref_ptr<osg::Node> terrainNode = loadTerrainModel(terrainModel);

    if (!terrainNode.valid())
    {
        terrainModel = terrainAlternative;
        terrainNode = loadTerrainModel(terrainModel);
    }

    if (!terrainNode.valid())
    {
        std::cerr << "WINSENTWindFarm: failed to load terrain model."
                  << " Tried configured terrain files in " << dataPath << std::endl;
        return false;
    }

    if (endsWith(terrainModel, ".obj"))
    {
        // OBJ is Y-up: swap Y/Z so it matches the Z-up frame used by
        // LiDAR/UAV workflow transforms and mast placement.
        osg::Matrixd swapYZ(1.0, 0.0, 0.0, 0.0,
                            0.0, 0.0, 1.0, 0.0,
                            0.0, 1.0, 0.0, 0.0,
                            0.0, 0.0, 0.0, 1.0);
        osg::ref_ptr<osg::MatrixTransform> terrainXform = new osg::MatrixTransform();
        terrainXform->setName("WINSENTTerrainAxisSwap");
        terrainXform->setMatrix(swapYZ);
        terrainXform->addChild(terrainNode.get());
        terrainNode = terrainXform;
        std::cerr << "WINSENTWindFarm: applied OBJ->ZUp axis mapping (swap Y/Z)" << std::endl;
    }

    terrainNode->setName("WINSENTTerrain");
    siteRoot_->addChild(terrainNode.get());
    std::cerr << "WINSENTWindFarm: loaded terrain model " << terrainModel << std::endl;

    // Keep terrain colors faithful to orthophoto and avoid dull/dark shading.
    // This affects only the terrain subtree.
    {
        osg::StateSet *terrainState = terrainNode->getOrCreateStateSet();
        terrainState->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
        osg::ref_ptr<osg::TexEnv> texEnv = new osg::TexEnv();
        texEnv->setMode(osg::TexEnv::REPLACE);
        terrainState->setTextureAttributeAndModes(0, texEnv.get(),
                                                  osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    }

    bool useTexGenForTerrain = false;

    // The .ive terrain variant is geometrically correct for this workflow, but
    // may miss a bound texture depending on runtime/material resolution.
    // Bind the orthophoto explicitly so terrain remains colored without COCONFIG.
    if (endsWith(terrainModel, ".ive"))
    {
        std::string textureFile = cfgString("TerrainTexture", dataPath + "/OrthoPic.jpg");
        osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile(textureFile);
        if (!image.valid())
        {
            textureFile = cfgString("TerrainTextureFallback", dataPath + "/OrthoPic.tiff");
            image = osgDB::readRefImageFile(textureFile);
        }

        if (image.valid())
        {
            osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(image.get());
            tex->setDataVariance(osg::Object::STATIC);
            tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
            tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
            tex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
            tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

            osg::StateSet *ss = terrainNode->getOrCreateStateSet();
            ss->setTextureAttributeAndModes(0, tex.get(), osg::StateAttribute::ON);
            useTexGenForTerrain = true;
            std::cerr << "WINSENTWindFarm: applied terrain texture " << textureFile << std::endl;
        }
        else
        {
            std::cerr << "WINSENTWindFarm: could not load terrain texture for .ive terrain" << std::endl;
        }
    }

    osg::ComputeBoundsVisitor cbv;
    terrainNode->accept(cbv);
    const osg::BoundingBoxd bb = cbv.getBoundingBox();
    if (bb.valid())
    {
        terrainMin_.set(bb.xMin(), bb.yMin(), bb.zMin());
        terrainMax_.set(bb.xMax(), bb.yMax(), bb.zMax());
        terrainBoundsValid_ = true;

        // The terrain assets come in two variants:
        // - OBJ: Y is up (small Y span, large Z span)
        // - IVE: Z is up (small Z span, large Y span)
        const double spanY = terrainMax_.y() - terrainMin_.y();
        const double spanZ = terrainMax_.z() - terrainMin_.z();
        terrainIsYUp_ = (spanY < spanZ);

        std::cerr << "WINSENTWindFarm: terrain bounds min=("
                  << terrainMin_.x() << ", " << terrainMin_.y() << ", " << terrainMin_.z()
                  << ") max=("
                  << terrainMax_.x() << ", " << terrainMax_.y() << ", " << terrainMax_.z()
                  << "), detected up axis=" << (terrainIsYUp_ ? "Y" : "Z") << std::endl;

        if (useTexGenForTerrain)
        {
            const double spanX = std::max(1e-6, terrainMax_.x() - terrainMin_.x());
            const double spanY = std::max(1e-6, terrainMax_.y() - terrainMin_.y());

            osg::ref_ptr<osg::TexGen> texGen = new osg::TexGen();
            texGen->setMode(osg::TexGen::OBJECT_LINEAR);
            texGen->setPlane(osg::TexGen::S, osg::Plane(1.0 / spanX, 0.0, 0.0, -terrainMin_.x() / spanX));
            texGen->setPlane(osg::TexGen::T, osg::Plane(0.0, 1.0 / spanY, 0.0, -terrainMin_.y() / spanY));

            osg::StateSet *ss = terrainNode->getOrCreateStateSet();
            ss->setTextureAttributeAndModes(0, texGen.get(), osg::StateAttribute::ON);
            std::cerr << "WINSENTWindFarm: enabled texture coordinate generation for terrain" << std::endl;
        }
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

    double offsetEast = 0.0;
    double offsetNorth = 0.0;
    double offsetUp = 0.0;
    if (terrainBoundsValid_)
    {
        offsetEast = terrainMin_.x() - topoMin.x();
        if (terrainIsYUp_)
        {
            offsetNorth = terrainMin_.z() - topoMin.y();
            offsetUp = terrainMin_.y() - topoMin.z();
        }
        else
        {
            offsetNorth = terrainMin_.y() - topoMin.y();
            offsetUp = terrainMin_.z() - topoMin.z();
        }
    }

    const osg::Vec4 mastColor(0.8f, 0.8f, 0.82f, 1.0f);

    osg::ref_ptr<osg::Group> mastGroup = new osg::Group();
    mastGroup->setName("WINSENTMasts");
    osg::ref_ptr<osg::Geode> mastGeode = new osg::Geode();
    mastGeode->setName("WINSENTMastGeode");

    for (const osg::Vec3d &base : mastBases)
    {
        const double alignedEast = base.x() + offsetEast;
        const double alignedNorth = base.y() + offsetNorth;
        const double alignedUp = base.z() + offsetUp;

        osg::ref_ptr<osg::Box> mastBox;
        if (terrainIsYUp_)
        {
            // local axes: (x=east, y=up, z=north)
            osg::Vec3 center(static_cast<float>(alignedEast),
                             static_cast<float>(alignedUp + mastHeight * 0.5),
                             static_cast<float>(alignedNorth));
            mastBox = new osg::Box(center, mastWidth, mastHeight, mastWidth);
        }
        else
        {
            // local axes: (x=east, y=north, z=up)
            osg::Vec3 center(static_cast<float>(alignedEast),
                             static_cast<float>(alignedNorth),
                             static_cast<float>(alignedUp + mastHeight * 0.5));
            mastBox = new osg::Box(center, mastWidth, mastWidth, mastHeight);
        }

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
