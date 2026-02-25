#ifndef WINSENTWIND_FARM_PLUGIN_H
#define WINSENTWIND_FARM_PLUGIN_H

/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include <cover/coVRPlugin.h>

#include <osg/Vec3d>
#include <osg/ref_ptr>

#include <string>
#include <vector>

namespace osg
{
class MatrixTransform;
class Node;
}

class WINSENT : public opencover::coVRPlugin
{
public:
    WINSENT();
    ~WINSENT() override;

    bool init() override;

private:
    bool loadTerrainLayer(const std::string &dataPath);
    bool loadMastLayer(const std::string &dataPath);

    osg::ref_ptr<osg::Node> loadTerrainModel(const std::string &terrainModelFile) const;

    static bool readCsvXYZ(const std::string &csvFile, std::vector<osg::Vec3d> &points);
    static bool readCsvBoundsXYZ(const std::string &csvFile, osg::Vec3d &minPoint, osg::Vec3d &maxPoint);

    osg::ref_ptr<osg::MatrixTransform> root_;
    osg::ref_ptr<osg::MatrixTransform> siteRoot_;
    osg::Vec3d terrainMin_{};
    osg::Vec3d terrainMax_{};
    bool terrainBoundsValid_ = false;
};

#endif
