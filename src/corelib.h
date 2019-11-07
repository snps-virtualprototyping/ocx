/******************************************************************************
 * Copyright Synopsys, licensed under the LGPL v2.1, see LICENSE for details
 ******************************************************************************/

#ifndef OCX_CORELIB_H
#define OCX_CORELIB_H

#include <iostream>
#include <dlfcn.h>
#include <gtest/gtest.h>

#include "ocx/ocx.h"

class corelib {
private:
    typedef ocx::core*  (createfunc)(ocx::u64, ocx::env&, const char*);
    typedef void        (deletefunc)(ocx::core*);

    const char* m_sopath;
    void*       m_handle;
    createfunc* m_create;
    deletefunc* m_delete;

    const char* const CREATE_SYM = "_ZN3ocx15create_instanceEyRNS_3envEPKc";
    const char* const DELETE_SYM = "_ZN3ocx15delete_instanceEPNS_4coreE";

    void do_construction() {
        ASSERT_NE(m_handle, nullptr)
            << "could not dlopen library " << m_sopath << "\n" << dlerror();

        m_create = (createfunc*)dlsym(m_handle, CREATE_SYM);
        m_delete = (deletefunc*)dlsym(m_handle, DELETE_SYM);

        ASSERT_NE(m_create, nullptr) <<  "binding create function failed";
        ASSERT_NE(m_delete, nullptr) << "binding delete function failed";
    }

    void do_destruction() {
        if (m_handle != nullptr) {
            int res = dlclose(m_handle);
            ASSERT_EQ(res, 0)
                << "dlclose failed with code " << res << " - " << dlerror();
        }
    }

public:
    corelib(const char* path, bool lazy = true): m_sopath(path),
        m_handle(dlopen(path, RTLD_LOCAL | (lazy ? RTLD_LAZY : RTLD_NOW))) {
        do_construction();
    }

    ~corelib() {
        do_destruction();
    }

    ocx::core* create_core(ocx::env& env, const char* variant,
                           ocx::u64 api_version = OCX_API_VERSION) {
        return m_create(api_version, env, variant);
    }

    void delete_core(ocx::core* core) {
        m_delete(core);
    }

};

#endif
