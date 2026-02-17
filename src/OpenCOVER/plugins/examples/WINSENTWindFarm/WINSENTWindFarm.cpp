/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */
#include "WINSENTWindFarm.h"

using namespace covise;
using namespace opencover;

WINSENT::WINSENT()
    : coVRPlugin("WINSENTWindFarm")
{
}

bool WINSENT::init()
{
    return true;
}

void WINSENT::preFrame()
{
}