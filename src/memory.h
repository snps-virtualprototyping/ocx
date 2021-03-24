/*******************************************************************************
* Copyright (C) 2021 Synopsys, Inc.
* This source code is licensed under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2 of the License
* or (at your option) any later version.
*******************************************************************************/

#ifndef MEMORY_H
#define MEMORY_H

#include <cstdlib>
#include <cstdio>

#include "ocx/ocx.h"

namespace ocx {

    class memory
    {
    private:
        u64 m_size;
        u8* m_memory;
        u8* m_buffer;

        memory() = delete;
        memory(const memory&) = delete;

    public:
        memory(u64 size, u64 alignment);
        virtual ~memory();

        inline u8* get_ptr()  const { return m_memory; }
        inline u64 get_size() const { return m_size; }

        void load(const char*);

        ocx::response transact(const ocx::transaction& tx);
    };

}

#endif
