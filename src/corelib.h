/******************************************************************************
 * Copyright Synopsys, licensed under the LGPL v2.1, see LICENSE for details
 ******************************************************************************/

#ifndef OCX_CORELIB_H
#define OCX_CORELIB_H

#include <iostream>
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

#ifdef _MSC_VER
    const char* const CREATE_SYM = "?create_instance@ocx@@YAPEAVcore@1@_KAEAVenv@1@PEBD@Z";
    const char* const DELETE_SYM = "?delete_instance@ocx@@YAXPEAVcore@1@@Z";
#else
    const char* const CREATE_SYM = "_ZN3ocx15create_instanceEmRNS_3envEPKc";
    const char* const DELETE_SYM = "_ZN3ocx15delete_instanceEPNS_4coreE";
#endif

    void do_construction() {
        ASSERT_NE(m_handle, nullptr)
            << "could not dlopen library " << m_sopath << "\n" << _dlerror();

        m_create = (createfunc*)_dlsym(m_handle, CREATE_SYM);
        m_delete = (deletefunc*)_dlsym(m_handle, DELETE_SYM);

        ASSERT_NE(m_create, nullptr) <<  "binding create function failed";
        ASSERT_NE(m_delete, nullptr) << "binding delete function failed";
    }

    void do_destruction() {
        if (m_handle != nullptr) {
            int res = _dlclose(m_handle);
            ASSERT_EQ(res, 0)
                << "dlclose failed with code " << res << " - " << _dlerror();
        }
    }

public:
    corelib(const char* path, bool lazy = true): m_sopath(path),
		m_handle(_dlopen(path, lazy)) {
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

private:
	static void* _dlopen(const char* path, bool lazy);
	static int _dlclose(void* handle);
	static void* _dlsym(void* handle, const char* sym);
	static const char* _dlerror();
};

#ifdef WIN32
#include <Windows.h>
#undef min
#undef max

void* corelib::_dlopen(const char* path, bool lazy) {
	(void)lazy;
	return (void*)LoadLibrary(path);
}

int corelib::_dlclose(void* handle) {
    return FreeLibrary((HMODULE)handle) != 0 ? 0 : 1;
}

void* corelib::_dlsym(void* handle, const char* sym) {
	return (void*)GetProcAddress((HMODULE)handle, sym);
}

const char* corelib::_dlerror() {
	DWORD dwErrorCode = GetLastError();
	static char buf[1024];
	DWORD cchMsg = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, 1024, NULL);
	return (cchMsg > 0) ? buf : "";
}

#else

#include <dlfcn.h>

void* corelib::_dlopen(const char* path, bool lazy) {
	return dlopen(path, RTLD_LOCAL | (lazy ? RTLD_LAZY : RTLD_NOW));
}

int corelib::_dlclose(void* handle) {
	return dlclose(handle);
}

void* corelib::_dlsym(void* handle, const char* sym) {
	return dlsym(handle, sym);
}

const char* corelib::_dlerror() { 
	return dlerror();  
}

#endif

#endif
