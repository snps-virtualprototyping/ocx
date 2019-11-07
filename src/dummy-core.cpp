/*******************************************************************************
* Copyright (C) 2019 Synopsys, Inc.
* This source code is licensed under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2 of the License
* or (at your option) any later version.
*******************************************************************************/

#include <iostream>

#include "ocx/ocx.h"

namespace ocx {

    core* create_instance(u64 api_version, env& e, const char* variant) {
        (void)e;
        std::cerr << "This is just a dummy OpenCpuX stub, which does not "
                  << "provide any functionality." << std::endl;
        std::cerr << "Requested model was '" << variant << "', "
                  << "API version " << api_version << std::endl;
        return nullptr;
    }

    void delete_instance(core* c) {
        (void)c;
    }

}
