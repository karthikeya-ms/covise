#ifndef WINSENTWIND_FARM_PLUGIN_H
#define WINSENTWIND_FARM_PLUGIN_H

/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

#include <cover/coVRPlugin.h>
#include <cover/coVRPluginSupport.h>
#include <cover/coInteractor.h>

class WINSENT : public opencover::coVRPlugin
{
    public:
        WINSENT();

        bool init() override;
        void preFrame() override;

    private:
        void loadCSVFile(const std::string &filename);
}