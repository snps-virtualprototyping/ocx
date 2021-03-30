/*******************************************************************************
* Copyright (C) 2021 Synopsys, Inc.
* This source code is licensed under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2 of the License
* or (at your option) any later version.
*******************************************************************************/

#ifdef _MSC_VER
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

#ifdef ERROR
#undef ERROR
#endif

#include "common.h"
#include "memory.h"

#include <inttypes.h>
#include <iostream>
#include <fstream>

namespace ocx {

    memory::memory(u64 size, u64 alignment) :
        m_size(size),
        m_memory(nullptr),
        m_buffer(nullptr) {
#ifdef _MSC_VER
        m_buffer = _aligned_malloc(size, alignment);
        m_memory = (u8*)m_buffer;
        ERROR_ON(m_memory == nullptr,
                 "Unable to allocate %" PRIu64 " bytes of memory\n", size);
#else
        const int p_flags = PROT_READ|PROT_WRITE|PROT_EXEC;
        const int m_flags = MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE;
        m_buffer = mmap(NULL, size + alignment, p_flags, m_flags, -1, 0);
        ERROR_ON(m_buffer == MAP_FAILED,
                 "Unable to reserve %" PRIu64 " bytes of memory\n", size);

        uintptr_t aligned_start = ((uintptr_t)m_buffer + (alignment - 1))
                                  & ~(alignment - 1);
        m_memory = (u8*)aligned_start;
#endif
    }

    memory::~memory() {
#ifdef _MSC_VER
        _aligned_free(m_buffer);
#else
        (void)munmap(m_buffer, m_size);
#endif
    }

    void memory::load(const char* path) {

        std::ifstream file(path, std::ios::binary);
        file.unsetf(std::ios::skipws);

        ERROR_ON(!file.good(), "unable to read %s", path);

        std::streampos file_size;
        file.seekg(0, std::ios::end);
        file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        ERROR_ON ((u64)file_size > m_size,
                  "file %s larger than memory size %" PRIu64, path, m_size);

        file.read((char*)m_memory, file_size);
        ERROR_ON(!file.good(), "unable to read %s", path);
    }

    ocx::response memory::transact(const ocx::transaction& tx) {
        if (tx.addr + tx.size >= m_size)
            return RESP_FAILED;

        if (tx.is_read)
            memcpy(tx.data, m_memory + tx.addr, tx.size);
        else
            memcpy(m_memory + tx.addr, tx.data, tx.size);

        return RESP_OK;
    }

    void memory::protect_page(u8* page_ptr, u64 page_addr) {
        (void)page_addr;
#ifdef _MSC_VER
         DWORD oldProtect;
         if (!VirtualProtect(page_ptr, 0x1000, PAGE_READONLY, &oldProtect))
             ERROR("failed to protect memory at %p - error %d",
                   page_ptr, GetLastError());
 #else
         mprotect(page_ptr, 0x1000, PROT_READ);
 #endif
    }
}
