/*******************************************************************************
* Copyright (C) 2019 Synopsys, Inc.
* This source code is licensed under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2 of the License
* or (at your option) any later version.
*******************************************************************************/

#include <iostream>
#include <thread>

#define OCX_DLL_EXPORT

#include "ocx/ocx.h"

namespace ocx {

    class dummycore: public core
    {
    public:
        dummycore():
            core(), m_num_insn() {
        }

        virtual ~dummycore() {
            return;
        }

        virtual const char* provider() override {
            return "ocx::dummycore";
        }

        virtual const char* arch() override {
            return "NONE";
        }

        virtual const char* arch_gdb() override {
            return "NONE";
        }

        virtual const char* arch_family() override {
            return "NONE";
        }

        virtual u64 page_size() override {
            return 4096;
        }

        virtual void set_id(u64 procid, u64 coreid) override {
            (void)procid;
            (void)coreid;
        }

        virtual u64 step(u64 num_insn) override {
            std::this_thread::sleep_for(std::chrono::microseconds(2));
            m_num_insn += num_insn;
            return num_insn;
        }

        virtual void stop() override {
            return;
        }

        virtual u64 insn_count() override {
            return m_num_insn;
        }

        virtual void reset() override {
            return;
        }

        virtual void interrupt(u64 irq, bool set) override {
            (void)irq;
            (void)set;
        }

        virtual void notified(u64 eventid) override {
            (void)eventid;
        }

        virtual u64 pc_regid() override {
            return 0;
        }

        virtual u64 sp_regid() override {
            return 0;
        }

        virtual u64 num_regs() override {
            return 1;
        }

        virtual size_t reg_size(u64 regid) override {
            (void)regid;
            return 4;
        }

        virtual const char* reg_name(u64 regid) override {
            (void)regid;
            return "NONE";
        }

        virtual bool read_reg(u64 regid, void* buf) override {
            (void)(regid);
            (void)(buf);
            return 0;
        }

        virtual bool write_reg(u64 regid, const void* buf) override {
            (void)regid;
            (void)buf;
            return 0;
        }

        virtual bool add_breakpoint(u64 vaddr) override {
            (void)vaddr;
            return false;
        }

        virtual bool remove_breakpoint(u64 vaddr) override {
            (void)vaddr;
            return false;
        }

        virtual bool add_watchpoint(u64 vaddr, u64 sz, bool iswr) override {
            (void)vaddr;
            (void)sz;
            (void)iswr;
            return false;
        }

        virtual bool remove_watchpoint(u64 vaddr, u64 sz, bool iswr) override {
            (void)vaddr;
            (void)sz;
            (void)iswr;
            return false;
        }

        virtual bool trace_basic_blocks(bool on) override {
            (void)on;
            return false;
        }

        virtual bool virt_to_phys(u64 vaddr, u64& paddr) override {
            (void)vaddr;
            (void)paddr;
            return false;
        }

        virtual void handle_syscall(int callno,
                                    std::shared_ptr<void> arg) override {
            (void)callno;
            (void)arg;
        }

        virtual u64 disassemble(u64 addr, char* buf, size_t sz) override {
            (void)addr;
            (void)buf;
            (void)sz;
            return 0;
        }

        virtual void invalidate_page_ptrs() override {
            return;
        }

        virtual void invalidate_page_ptr(u64 page_paddr) override {
            (void)page_paddr;
        }

        virtual void invalidate_page_ptrs(u64 page_paddr_start, u64 size) override {
            (void)page_paddr_start;
            (void)size;
        }

        virtual void tb_flush() override {
            return;
        }

        virtual void tb_flush_page(u64 start, u64 end) override {
            (void)start;
            (void)end;
            return;
        }

    private:
        u64 m_num_insn;
    };

    core* create_instance(u64 api_version, env& e, const char* variant) {
        (void)(e);

        static bool warned = false;
        if (!warned) {
            std::cerr << "This is just a dummy OpenCpuX stub, which does not "
                      << "provide any functionality." << std::endl;
            std::cerr << "Requested model was '" << variant << "', "
                      << "API version " << api_version << std::endl;
            warned = true;
        }

        if (api_version == OCX_API_VERSION_20201012 ||
            api_version == OCX_API_VERSION)
            return new dummycore();
        else return nullptr;
    }

    void delete_instance(core* cpu) {
        if (cpu == nullptr)
            return;

        dummycore* dcpu = dynamic_cast<dummycore*>(cpu);
        if (dcpu == nullptr) {
            std::cerr << "attempt to delete foreign core " << cpu->provider()
                      << std::endl;
            abort();
        }

        delete dcpu;
    }

}
