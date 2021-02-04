/*******************************************************************************
* Copyright (C) 2019 Synopsys, Inc.
* This source code is licensed under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2 of the License
* or (at your option) any later version.
*******************************************************************************/

#ifndef OCX_API_H
#define OCX_API_H

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory>

#define OCX_API_VERSION 20201012ull

#ifdef _MSC_VER
#  ifdef OCX_STATIC
#    define OCX_API
#  else
#    ifdef OCX_DLL_EXPORT
#      define OCX_API __declspec(dllexport)
#    else
#      define OCX_API __declspec(dllimport)
#    endif
#  endif
#else
#  define OCX_API
#endif

namespace ocx {

    typedef uint8_t  u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;

    struct transaction {
        u64 addr;
        u64 size;
        u8* data;
        bool is_read;
        bool is_user;
        bool is_secure;
        bool is_insn;
        bool is_excl;
        bool is_lock;
        bool is_port;
        bool is_debug;
    };

    enum response {
        RESP_OK = 0,
        RESP_FAILED,
        RESP_NOT_EXCLUSIVE,
        RESP_ADDRESS_ERROR,
        RESP_COMMAND_ERROR,
    };

    enum hint_kind {
        HINT_YIELD = 0,
        HINT_WFI,
        HINT_WFE,
        HINT_SEV,
        HINT_SEVL,
    };

    class env
    {
    public:
        virtual ~env() {}

        virtual u8* get_page_ptr_r(u64 page_paddr) = 0;
        virtual u8* get_page_ptr_w(u64 page_paddr) = 0;

        virtual void protect_page(u8* page_ptr, u64 page_addr) = 0;

        virtual response transport(const transaction& tx) = 0;
        virtual void signal(u64 sigid, bool set) = 0;

        virtual void broadcast_syscall(int callno, std::shared_ptr<void> arg,
                                       bool async) = 0;

        virtual u64 get_time_ps() = 0;
        virtual const char* get_param(const char* name) = 0;

        virtual void notify(u64 eventid, u64 time_ps) = 0;
        virtual void cancel(u64 eventid) = 0;

        virtual void hint(hint_kind kind) = 0;

        virtual void handle_begin_basic_block(u64 vaddr) = 0;
        virtual bool handle_breakpoint(u64 vaddr) = 0;
        virtual bool handle_watchpoint(u64 vaddr, u64 size, u64 data,
                                       bool iswr) = 0;
    };

    class core
    {
    protected:
        virtual ~core() {}

    public:
        virtual const char* provider() = 0;

        virtual const char* arch() = 0;
        virtual const char* arch_gdb() = 0;
        virtual const char* arch_family() = 0;

        virtual u64 page_size() = 0;

        virtual void set_id(u64 procid, u64 coreid) = 0;

        virtual u64 step(u64 num_insn) = 0;
        virtual void stop() = 0;
        virtual u64 insn_count() = 0;

        virtual void reset() = 0;
        virtual void interrupt(u64 irq, bool set) = 0;

        virtual void notified(u64 eventid) = 0;

        virtual u64 pc_regid() = 0;
        virtual u64 sp_regid() = 0;
        virtual u64 num_regs() = 0;

        virtual size_t      reg_size(u64 regid) = 0;
        virtual const char* reg_name(u64 regid) = 0;

        virtual bool read_reg(u64 regid, void* buf) = 0;
        virtual bool write_reg(u64 regid, const void* buf) = 0;

        virtual bool add_breakpoint(u64 vaddr) = 0;
        virtual bool remove_breakpoint(u64 vaddr) = 0;

        virtual bool add_watchpoint(u64 vaddr, u64 size, bool iswr) = 0;
        virtual bool remove_watchpoint(u64 vaddr, u64 size, bool iswr) = 0;

        virtual bool trace_basic_blocks(bool on) = 0;

        virtual bool virt_to_phys(u64 vaddr, u64& paddr) = 0;

        virtual void handle_syscall(int callno, std::shared_ptr<void> arg) = 0;

        virtual u64 disassemble(u64 addr, char* tgt, size_t tgtsz) = 0;

        virtual void invalidate_page_ptrs() = 0;
        virtual void invalidate_page_ptr(u64 page_paddr) = 0;

        virtual void tb_flush() = 0;
        virtual void tb_flush_page(u64 start, u64 end) = 0;
    };

    extern OCX_API core* create_instance(u64 ver, env& e, const char* variant);
    extern OCX_API void  delete_instance(core* c);

}

#endif
