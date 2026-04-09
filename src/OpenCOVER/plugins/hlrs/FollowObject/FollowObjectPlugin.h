/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#ifndef FOLLOW_OBJECT_PLUGIN_H
#define FOLLOW_OBJECT_PLUGIN_H

#include <cover/coVRPlugin.h>

#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/Vec3d>

#include <map>
#include <string>
#include <vector>

class FollowObjectPlugin : public opencover::coVRPlugin
{
public:
    FollowObjectPlugin();
    ~FollowObjectPlugin() override;

    void addObject(const opencover::RenderObject *container,
                   osg::Group *root,
                   const opencover::RenderObject *geometry,
                   const opencover::RenderObject *normals,
                   const opencover::RenderObject *colors,
                   const opencover::RenderObject *texture) override;

    void addNode(osg::Node *node, const opencover::RenderObject *ro = nullptr) override;
    void removeObject(const char *objName, bool replaceFlag) override;
    void setTimestep(int t) override;

private:
    struct Instance
    {
        std::string key;
        std::string modelFile;
        std::vector<osg::Vec3d> positions;
        bool followHeading = true;
        double scale = 1.0;
        osg::Vec3d offset = osg::Vec3d(0.0, 0.0, 0.0);

        osg::ref_ptr<osg::Group> root;
        osg::ref_ptr<osg::MatrixTransform> pose;
        osg::ref_ptr<osg::Node> model;
    };

    using InstanceMap = std::map<std::string, Instance>;

    InstanceMap m_instances;

    bool isFollowObject(const opencover::RenderObject *obj) const;
    std::vector<osg::Vec3d> extractPositions(const opencover::RenderObject *geometry) const;
    osg::Vec3d parseVec3Attribute(const opencover::RenderObject *obj, const char *name, const osg::Vec3d &fallback) const;
    double parseDoubleAttribute(const opencover::RenderObject *obj, const char *name, double fallback) const;
    bool parseBoolAttribute(const opencover::RenderObject *obj, const char *name, bool fallback) const;

    osg::ref_ptr<osg::Node> loadModel(const std::string &modelFile) const;
    osg::ref_ptr<osg::Group> createPlaceholderModel() const;
    void updateInstance(Instance &instance, int timestep);
    void removeInstance(Instance &instance);
};

#endif
