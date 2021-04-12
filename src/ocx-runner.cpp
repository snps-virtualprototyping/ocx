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
#include <chrono>

using namespace std;
using namespace std::chrono;

namespace ocx {
    class runenv : public env {
    public:
        runenv(memory &mem, u64 uart, map<string, string>& params) :
            m_mem(mem),
            m_uart(uart),
            m_params(params),
            m_start(system_clock::now())
        {}

        ~runenv() {}

        u8* get_page_ptr_r(u64 page_paddr) override {
            if (page_paddr >= m_mem.get_size())
                return nullptr;
            if (page_paddr == (m_uart & ~0xfff))
                return nullptr;

            return m_mem.get_ptr() + page_paddr;
        }

        u8* get_page_ptr_w(u64 page_paddr) override {
            if (page_paddr >= m_mem.get_size())
                return nullptr;
            if (page_paddr == (m_uart & ~0xfff))
                return nullptr;

            return m_mem.get_ptr() + page_paddr;
        }

        void protect_page(u8* page_ptr, u64 page_addr) override {
            m_mem.protect_page(page_ptr, page_addr);
        }

        response transport(const transaction& tx) override {
            if (tx.addr == m_uart) {
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
            auto now = system_clock::now();
            return duration_cast<nanoseconds>(m_start - now).count() * 1000ull;
        }

        const char* get_param(const char* name) override {
            auto it = m_params.find(name);
            if (it != m_params.end())
                return it->second.c_str();
            else
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
        memory& m_mem;
        u64     m_uart;
        map<string, string> m_params;
        time_point<system_clock> m_start;
    };
}

static void usage(const char* name) {
    fprintf(stderr, "Usage: %s -b file [-m size] [-a align] [-u uart]", name);
    fprintf(stderr, "[-r pc] [-n num] [-q num] {-D x=y}\n");
    fprintf(stderr, "<ocx-lib> <variant>\n");
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  -b <file>   raw binary image to load into memory\n");
    fprintf(stderr, "  -m <size>   simulated memory size (in bytes)\n");
    fprintf(stderr, "  -a <align>  memory alignment (in bytes)\n");
    fprintf(stderr, "  -u <addr>   uart address\n");
    fprintf(stderr, "  -r <addr>   reset pc value\n");
    fprintf(stderr, "  -n <cores>  number of core instances\n");
    fprintf(stderr, "  -q <n>      number of instructions per quantum\n");
    fprintf(stderr, "  -D <param> = <val>   config param setting\n");
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
    unsigned int uart = 0x40000000;
    unsigned int reset = 0x0;
    unsigned int ncores = 1;
    vector<string> params;
    map<string, string> params_map;

    int c; // parse command line
    while ((c = getopt(argc, argv, "b:m:n:q:u:r:D:h")) != -1) {
        switch(c) {
        case 'b': binary    = optarg; break;
        case 'm': memsize   = strtol(optarg, nullptr, 0); break;
        case 'a': memalign  = strtol(optarg, nullptr, 0); break;
        case 'q': quantum   = strtol(optarg, nullptr, 0); break;
        case 'n': ncores    = strtol(optarg, nullptr, 0); break;
        case 'u': uart      = strtol(optarg, nullptr, 0); break;
        case 'r': reset     = strtol(optarg, nullptr, 0); break;
        case 'D': params.push_back(optarg);               break;
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

    for (auto& param : params) {
        istringstream s(param);
        string name;
        string value;
        if (!getline(s, name, '=') || !getline(s, value)) {
            fprintf(stderr, "invalid parameter setting %s\n", param.c_str());
            usage(argv[0]);
            return EXIT_FAILURE;
        }
        params_map[name] = value;
    }

    ocx_lib_path = argv[optind];
    ocx_variant =  argv[optind + 1];

    corelib cl(ocx_lib_path);
    ocx::memory mem(memsize, memalign);
    printf("Allocated 0x%" PRIx64 " bytes at 0x%p\n",
           mem.get_size(), mem.get_ptr());
    printf("Placing UART at 0x%08" PRIx32 "\n", uart);

    mem.load(binary);
    printf("Loaded file %s into memory\n", binary);

    ocx::runenv env(mem, uart, params_map);

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
        threads.emplace_back(run_core, c, quantum, reset);

    for (auto& t : threads)
        t.join();

    for (auto c : cores)
        cl.delete_core(c);

    return EXIT_SUCCESS;
}
