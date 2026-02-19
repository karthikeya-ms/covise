#ifndef WINSENTWIND_FARM_PLUGIN_H
#define WINSENTWIND_FARM_PLUGIN_H

/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include <cover/coVRPlugin.h>

#include <osg/ref_ptr>

#include <string>

namespace osg
{
class MatrixTransform;
class Node;
}

class WINSENT : public opencover::coVRPlugin
{
public:
    enum class CoordinateMode
    {
        ZUp,
        YUp
    };

    WINSENT();
    ~WINSENT() override;

    bool init() override;
    void preFrame() override;

private:
    bool loadTerrainLayer(const std::string &dataPath, CoordinateMode mode);

    osg::ref_ptr<osg::Node> loadTerrainModel(const std::string &terrainModelFile) const;
    static CoordinateMode readCoordinateMode(const std::string &modeText);

    osg::ref_ptr<osg::MatrixTransform> root_;
};

#endif
