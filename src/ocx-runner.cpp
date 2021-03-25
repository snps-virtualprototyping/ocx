/*******************************************************************************
* Copyright (C) 2021 Synopsys, Inc.
* This source code is licensed under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2 of the License
* or (at your option) any later version.
*******************************************************************************/

#include "ocx/ocx.h"

#include "corelib.h"
#include "memory.h"
#include "getopt.h"

#ifdef ERROR
#undef ERROR
#endif

#include "common.h"

#include <inttypes.h>
#include <vector>
#include <thread>

using namespace std;

namespace ocx {
    class runenv : public env {
    public:
        runenv(memory &mem) :
            m_mem(mem)
        {}

        ~runenv() {}

        u8* get_page_ptr_r(u64 page_paddr) override {
            if (page_paddr >= m_mem.get_size())
                return nullptr;
            return m_mem.get_ptr() + page_paddr;
        }

        u8* get_page_ptr_w(u64 page_paddr) override {
            if (page_paddr >= m_mem.get_size())
                return nullptr;
            return m_mem.get_ptr() + page_paddr;
        }

        void protect_page(u8* page_ptr, u64 page_addr) override {
            (void)page_ptr;
            (void)page_addr;
            abort();
        }

        response transport(const transaction& tx) override {
            if (tx.addr == 0x40000000) {
                if (tx.is_read || tx.size != 4)
                    return RESP_FAILED;
                putchar(*(u32*)tx.data);
                return RESP_OK;
            }
            return m_mem.transact(tx);
        };

        void signal(u64 sigid, bool set) override {
            (void)sigid;
            (void)set;
            abort();
        }

        void broadcast_syscall(int callno, shared_ptr<void> arg,
                               bool async) override {
            (void)callno;
            (void)arg;
            (void)async;
            abort();
        }

        u64 get_time_ps() override {
            abort();
        }

        const char* get_param(const char* name) override {
            (void)name;
            return nullptr;
        }

        void notify(u64 eventid, u64 time_ps) override {
            (void)eventid;
            (void)time_ps;
            abort();
        };

        void cancel(u64 eventid) override {
            (void)eventid;
            abort();
        }

        void hint(hint_kind kind) override {
            (void)kind;
        }

        void handle_begin_basic_block(u64 vaddr) override {
            (void)vaddr;
        }

        bool handle_breakpoint(u64 vaddr) override {
            (void)vaddr;
            abort();
        }

        bool handle_watchpoint(u64 vaddr, u64 size, u64 data,
                               bool iswr) override {
            (void)vaddr;
            (void)size;
            (void)data;
            (void)iswr;
            abort();
        }

    private:
        memory &m_mem;
    };
}

static void usage(const char* name) {
    fprintf(stderr, "Usage: %s -b file [-m size] ", name);
    fprintf(stderr, "[-n num] [-q num] <ocx-lib> <variant>\n");
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  -b <file>   raw binary image to load into memory\n");
    fprintf(stderr, "  -m <size>   simulated memory size (in bytes)\n");
    fprintf(stderr, "  -a <align>  memory alignment (in bytes)\n");
    fprintf(stderr, "  -n <cores>  number of core instances\n");
    fprintf(stderr, "  -q <n>      number of instructions per quantum\n");
    fprintf(stderr, "  <ocx-lib>   the OCX core library to load\n");
    fprintf(stderr, "  <variant>   the OCX core variant to instantiate\n");
}

static void run_core(ocx::core* c, ocx::u64 quantum, ocx::u64 reset_pc) {

    c->write_reg(c->pc_regid(), &reset_pc);

    ocx::u64 overshoot = 0;
    for (;;) {
        overshoot = c->step(quantum - overshoot);
        if (overshoot >= quantum)
            overshoot -= quantum;
    }
}

int main(int argc, char** argv) {
    char* binary = NULL;
    char* ocx_lib_path = NULL;
    char* ocx_variant = NULL;
    unsigned int memsize = 0x08000000; // 128MB
    unsigned int memalign = 0x1000;    // 4K
    unsigned int quantum = 1000000;    // 1M instructions
    unsigned int ncores = 1;

    int c; // parse command line
    while ((c = getopt(argc, argv, "b:m:n:q:h")) != -1) {
        switch(c) {
        case 'b': binary    = optarg; break;
        case 'm': memsize   = atoi(optarg); break;
        case 'a': memalign  = atoi(optarg); break;
        case 'q': quantum   = atoi(optarg); break;
        case 'n': ncores    = atoi(optarg); break;
        case 'h': usage(argv[0]); return EXIT_SUCCESS;
        default : usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (binary == nullptr) {
        fprintf(stderr, "binary file must be specified\n");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (optind + 1 >= argc) {
        fprintf(stderr, "ocx library and variant must be specified\n");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    ocx_lib_path = argv[optind];
    ocx_variant =  argv[optind + 1];

    corelib cl(ocx_lib_path);
    ocx::memory mem(memsize, memalign);
    printf("Allocated 0x%" PRIx64 " bytes at 0x%p\n",
           mem.get_size(), mem.get_ptr());

    mem.load(binary);
    printf("Loaded file %s into memory\n", binary);

    ocx::runenv env(mem);

    vector<ocx::core*> cores;
    for (unsigned int i = 0; i < ncores; ++i) {
        ocx::core* c = cl.create_core(env, ocx_variant, OCX_API_VERSION);
        if (c == 0) {
            fprintf(stderr, "Failed to create OCX core variant %s\n",
                    ocx_variant);
            return EXIT_FAILURE;
        }

        printf("Created OCX core %s (%s)\n", c->arch(), c->provider());
        cores.push_back(c);
    }

    printf("Starting simulation with quantum %u\n", quantum);

    vector<thread> threads;
    for (auto c : cores)
        threads.emplace_back(run_core, c, quantum, 0);

    for (auto& t : threads)
        t.join();

    for (auto c : cores)
        cl.delete_core(c);

    return EXIT_SUCCESS;
}
