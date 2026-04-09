/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include "FollowObjectPlugin.h"

#include <cover/RenderObject.h>
#include <cover/coVRAnimationManager.h>
#include <cover/coVRPluginSupport.h>

#include <osg/Geode>
#include <osg/Matrix>
#include <osg/Quat>
#include <osg/ShapeDrawable>
#include <osgDB/ReadFile>

#include <algorithm>
#include <cctype>
#include <sstream>

using namespace opencover;

namespace
{
const char *FollowAttribute = "FOLLOW_OBJECT";
}

FollowObjectPlugin::FollowObjectPlugin()
    : coVRPlugin(COVER_PLUGIN_NAME)
{
}

FollowObjectPlugin::~FollowObjectPlugin()
{
    for (auto &entry : m_instances)
    {
        removeInstance(entry.second);
    }
    m_instances.clear();
}

void FollowObjectPlugin::addObject(const RenderObject *container,
                                   osg::Group *root,
                                   const RenderObject *geometry,
                                   const RenderObject *normals,
                                   const RenderObject *colors,
                                   const RenderObject *texture)
{
    (void)root;
    (void)normals;
    (void)colors;
    (void)texture;

    if (!geometry || !isFollowObject(geometry))
    {
        return;
    }

    const char *name = container && container->getName() ? container->getName() : geometry->getName();
    if (!name)
    {
        return;
    }

    removeObject(name, true);

    Instance instance;
    instance.key = name;
    instance.positions = extractPositions(geometry);
    if (instance.positions.empty())
    {
        return;
    }

    if (const char *modelFile = geometry->getAttribute("FOLLOW_MODEL"))
    {
        instance.modelFile = modelFile;
    }
    instance.scale = parseDoubleAttribute(geometry, "FOLLOW_SCALE", 1.0);
    instance.offset = parseVec3Attribute(geometry, "FOLLOW_OFFSET", osg::Vec3d(0.0, 0.0, 0.0));
    instance.followHeading = parseBoolAttribute(geometry, "FOLLOW_HEADING", true);

    instance.model = loadModel(instance.modelFile);
    if (!instance.model)
    {
        return;
    }

    instance.pose = new osg::MatrixTransform();
    instance.pose->setName("FollowObjectPose_" + instance.key);
    instance.pose->setDataVariance(osg::Object::DYNAMIC);
    instance.pose->setCullingActive(false);
    instance.pose->addChild(instance.model.get());

    instance.root = new osg::Group();
    instance.root->setName("FollowObjectRoot_" + instance.key);
    instance.root->setDataVariance(osg::Object::DYNAMIC);
    instance.root->setCullingActive(false);
    instance.root->addChild(instance.pose.get());
    cover->getObjectsRoot()->addChild(instance.root.get());

    updateInstance(instance, coVRAnimationManager::instance()->getAnimationFrame());
    m_instances[instance.key] = instance;
}

void FollowObjectPlugin::addNode(osg::Node *node, const RenderObject *ro)
{
    if (!node || !isFollowObject(ro))
    {
        return;
    }

    node->setNodeMask(0x0);
}

void FollowObjectPlugin::removeObject(const char *objName, bool replaceFlag)
{
    (void)replaceFlag;

    if (!objName)
    {
        return;
    }

    auto it = m_instances.find(objName);
    if (it == m_instances.end())
    {
        return;
    }

    removeInstance(it->second);
    m_instances.erase(it);
}

void FollowObjectPlugin::setTimestep(int t)
{
    for (auto &entry : m_instances)
    {
        updateInstance(entry.second, t);
    }
}

bool FollowObjectPlugin::isFollowObject(const RenderObject *obj) const
{
    if (!obj)
    {
        return false;
    }

    const char *attr = obj->getAttribute(FollowAttribute);
    return attr && *attr;
}

std::vector<osg::Vec3d> FollowObjectPlugin::extractPositions(const RenderObject *geometry) const
{
    std::vector<osg::Vec3d> positions;

    auto readPoint = [](const RenderObject *obj, osg::Vec3d &pos) {
        if (!obj)
        {
            return false;
        }

        const float *x = obj->getFloat(Field::X);
        const float *y = obj->getFloat(Field::Y);
        const float *z = obj->getFloat(Field::Z);
        if (!x || !y || !z)
        {
            return false;
        }

        pos.set(x[0], y[0], z[0]);
        return true;
    };

    if (geometry->isSet())
    {
        positions.reserve(geometry->getNumElements());
        for (size_t i = 0; i < geometry->getNumElements(); ++i)
        {
            osg::Vec3d pos;
            if (readPoint(geometry->getElement(i), pos))
            {
                positions.push_back(pos);
            }
        }
        return positions;
    }

    osg::Vec3d pos;
    if (readPoint(geometry, pos))
    {
        positions.push_back(pos);
    }
    return positions;
}

osg::Vec3d FollowObjectPlugin::parseVec3Attribute(const RenderObject *obj, const char *name, const osg::Vec3d &fallback) const
{
    if (!obj)
    {
        return fallback;
    }

    const char *attr = obj->getAttribute(name);
    if (!attr)
    {
        return fallback;
    }

    std::istringstream in(attr);
    osg::Vec3d result = fallback;
    if (!(in >> result.x() >> result.y() >> result.z()))
    {
        return fallback;
    }
    return result;
}

double FollowObjectPlugin::parseDoubleAttribute(const RenderObject *obj, const char *name, double fallback) const
{
    if (!obj)
    {
        return fallback;
    }

    const char *attr = obj->getAttribute(name);
    if (!attr || !*attr)
    {
        return fallback;
    }

    std::istringstream in(attr);
    double value = fallback;
    if (!(in >> value))
    {
        return fallback;
    }
    return value;
}

bool FollowObjectPlugin::parseBoolAttribute(const RenderObject *obj, const char *name, bool fallback) const
{
    if (!obj)
    {
        return fallback;
    }

    const char *attr = obj->getAttribute(name);
    if (!attr || !*attr)
    {
        return fallback;
    }

    std::string value(attr);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (value == "1" || value == "true" || value == "on")
    {
        return true;
    }
    if (value == "0" || value == "false" || value == "off")
    {
        return false;
    }

    return fallback;
}

osg::ref_ptr<osg::Node> FollowObjectPlugin::loadModel(const std::string &modelFile) const
{
    if (!modelFile.empty())
    {
        if (osg::ref_ptr<osg::Node> model = osgDB::readNodeFile(modelFile))
        {
            return model;
        }
    }

    return createPlaceholderModel();
}

osg::ref_ptr<osg::Group> FollowObjectPlugin::createPlaceholderModel() const
{
    auto addPart = [](osg::Group *group, osg::Shape *shape, const osg::Vec4 &color) {
        osg::ref_ptr<osg::ShapeDrawable> drawable = new osg::ShapeDrawable(shape);
        drawable->setColor(color);
        osg::ref_ptr<osg::Geode> geode = new osg::Geode();
        geode->addDrawable(drawable.get());
        group->addChild(geode.get());
    };

    osg::ref_ptr<osg::Group> model = new osg::Group();
    model->setName("FollowObjectPlaceholder");
    model->setDataVariance(osg::Object::DYNAMIC);
    model->setCullingActive(false);

    const osg::Vec4 bodyColor(0.18f, 0.22f, 0.28f, 1.0f);
    addPart(model.get(), new osg::Box(osg::Vec3(0.0f, 0.0f, 0.0f), 0.3f, 0.3f, 0.3f), bodyColor);

    return model;
}

void FollowObjectPlugin::updateInstance(Instance &instance, int timestep)
{
    if (!instance.pose || instance.positions.empty())
    {
        return;
    }

    const int clampedTimestep = std::max(0, std::min(timestep, static_cast<int>(instance.positions.size()) - 1));
    const osg::Vec3d pos = instance.positions[clampedTimestep];

    if (instance.followHeading && instance.positions.size() > 1)
    {
        osg::Vec3d dir;
        if (clampedTimestep + 1 < static_cast<int>(instance.positions.size()))
        {
            dir = instance.positions[clampedTimestep + 1] - pos;
        }
        else
        {
            dir = pos - instance.positions[clampedTimestep - 1];
        }

        if (dir.length2() > 1e-12)
        {
            dir.normalize();
            instance.rotation.makeRotate(osg::Vec3d(1.0, 0.0, 0.0), dir);
        }
    }

    const osg::Matrix matrix =
        osg::Matrix::translate(pos)
        * osg::Matrix::rotate(instance.rotation)
        * osg::Matrix::scale(instance.scale, instance.scale, instance.scale)
        * osg::Matrix::translate(instance.offset);

    instance.pose->setMatrix(matrix);
}

void FollowObjectPlugin::removeInstance(Instance &instance)
{
    if (!instance.root)
    {
        return;
    }

    while (instance.root->getNumParents() > 0)
    {
        osg::Group *parent = instance.root->getParent(0);
        if (!parent)
        {
            break;
        }
        parent->removeChild(instance.root.get());
    }
}

COVERPLUGIN(FollowObjectPlugin)
