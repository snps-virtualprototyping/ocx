/*******************************************************************************
* Copyright (C) 2019 Synopsys, Inc.
* This source code is licensed under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2 of the License
* or (at your option) any later version.
*******************************************************************************/

#include <string>
#include <map>
#include <vector>
#include <stdlib.h>
#include <functional>

#include <ocx/ocx.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "corelib.h"

using namespace ocx;

namespace ocx {

    bool operator == (const transaction& ta, const transaction& tb) {
        if (ta.addr != tb.addr)
            return false;
        if (ta.size != tb.size)
            return false;
        if (ta.is_read != tb.is_read)
            return false;
        if (ta.is_debug != tb.is_debug)
            return false;
        if (!ta.is_read && memcmp(ta.data, tb.data, ta.size) != 0)
            return false;
        return true;
    }

}

static const char* LIBRARY_PATH = "<library path missing>";
static const char* CORE_VARIANT = "<variant missing>";

class mock_env : public ocx::env {
public:
    ~mock_env() {}
    MOCK_METHOD1(get_page_ptr_r, u8*(u64));
    MOCK_METHOD1(get_page_ptr_w, u8*(u64));
    MOCK_METHOD2(protect_page, void(u8*,u64));
    MOCK_METHOD1(transport, response(const transaction&));
    MOCK_METHOD2(signal, void(u64, bool));
    MOCK_METHOD3(broadcast_syscall, void(int, std::shared_ptr<void>, bool));
    MOCK_METHOD0(get_time_ps, u64());
    MOCK_METHOD2(notify, void(u64, u64));
    MOCK_METHOD1(cancel, void(u64));
    MOCK_METHOD1(hint, void(ocx::hint_kind));
    MOCK_METHOD1(handle_breakpoint, bool(u64));
    MOCK_METHOD4(handle_watchpoint, bool(u64, u64, u64, bool));
    MOCK_METHOD1(handle_begin_basic_block, void(u64));
    MOCK_METHOD1(get_param, const char*(const char*));
};

void free_nop_code(void* buf) {
#ifdef _MSC_VER
    _aligned_free(buf);
#else
    free(buf);
#endif
}

void* alloc_nop_code(size_t sz) {
#ifdef _MSC_VER
    return _aligned_malloc(sz, 0x1000);
#else
    return valloc(sz);
#endif
}

void* prepare_nop_code(size_t code_size, const std::string& arch) {
    void* result = alloc_nop_code(code_size);

    const char* nop;
    size_t nopsz;

    // https://en.wikipedia.org/wiki/NOP_(code) has a list of NOP insns

    if (arch.find("ARMv8") == 0) {
        nop = "\x1f\x20\x03\xd5";
        nopsz = 4;
    } else if (arch.find("ARMv7") == 0) {
        nop = "\x00\x00\x00\x00";
        nopsz = 4;
    } else if (arch == "openrisc") {
        nop = "\x00\x00\x00\x15";
        nopsz = 4;
    } else if (arch == "X86") {
        nop = "\x90";
        nopsz = 1;
    } else if (arch == "riscv") {
        nop = "\x13\x00\x00\x00";
        nopsz = 4;
    } else {
        std::cerr << "unknown architecture " << arch << std::endl;
        free_nop_code(result);
        return nullptr;
    }

    char* p = (char*)result;
    for (size_t i = 0; i < code_size; i += nopsz) {
        memcpy(p, nop, nopsz);
        p += nopsz;
    }

    return result;
}

TEST(ocx_basic, load_library) {
    {
        corelib cl(LIBRARY_PATH, false);
    }

#ifdef WIN32
    void* handle = GetModuleHandle(LIBRARY_PATH);
#else
	void* handle = dlopen(LIBRARY_PATH, RTLD_LOCAL | RTLD_NOW | RTLD_NOLOAD);
#endif
    EXPECT_EQ(handle, nullptr)
        << "unloading shared library " << LIBRARY_PATH
        << " seems to have failed";
}

TEST(ocx_basic, instantiate_core) {
    using ::testing::Return;
    using ::testing::_;
    ::testing::NiceMock<mock_env> env;
    ON_CALL(env, get_param(_)).WillByDefault(Return(nullptr));

    corelib cl(LIBRARY_PATH);
    ocx::core* c = cl.create_core(env, CORE_VARIANT);
    ASSERT_NE(c, nullptr)
        << "failed to create core variant " << CORE_VARIANT;

    std::string arch = c->arch();
    EXPECT_FALSE(arch.empty()) << "core reports empty architecture";

    std::string arch_family = c->arch_family();
    EXPECT_FALSE(arch_family.empty()) << "core reports empty architecture family";
    u64 page_size = c->page_size();
    EXPECT_GT(page_size, 0) << "core reports zero page size";
    EXPECT_EQ(page_size ^ (page_size - 1), page_size + (page_size - 1))
        << "page size " << page_size << " is not a power of 2";

    c->set_id(1, 0);
    cl.delete_core(c);
}

TEST(ocx_basic, mismatched_version) {
    corelib cl(LIBRARY_PATH);
    mock_env env;
    ocx::core* c = cl.create_core(env, CORE_VARIANT, 0);
    ASSERT_EQ(c, nullptr)
        << "library returned core instance despite API version mismatch";
}

TEST(ocx_basic, older_version) {
    corelib cl(LIBRARY_PATH);
    mock_env env;
    ocx::core* c = cl.create_core(env, CORE_VARIANT, OCX_API_VERSION_20201012);
    ASSERT_NE(c, nullptr)
        << "library returned no core instance for API version OCX_API_VERSION_20201012";
}

class ocx_core : public ::testing::Test
{
private:
    corelib   m_cl;

protected:
    ::testing::NiceMock<mock_env> env;
    ocx::core *c;

    ocx_core():
        m_cl(LIBRARY_PATH),
        c(nullptr) {
    }

    void SetUp() override {
        c = m_cl.create_core(env, CORE_VARIANT);
        ASSERT_NE(c, nullptr) <<
                "failed to create core variant " << CORE_VARIANT;
    }

    void TearDown() override {
        if (c)
            m_cl.delete_core(c);
    }
};

TEST_F(ocx_core, registers_basic) {
    u64 num_regs = c->num_regs();
    ASSERT_NE(0, num_regs) << "core has no registers";

    u64 sp_reg = c->sp_regid();
    u64 pc_reg = c->pc_regid();

    EXPECT_LT(sp_reg, num_regs) << "SP regid out of bounds";
    EXPECT_LT(pc_reg, num_regs) << "PC regid out of bounds";
    EXPECT_NE(sp_reg, pc_reg) << "PC and SP regid clash";
}

TEST_F(ocx_core, register_names_unique) {
    using namespace std;
    map<string, u64> reg_names;
    u64 num_regs = c->num_regs();

    for (u64 i = 0; i < num_regs; ++i) {
        string reg_name = c->reg_name(i);
        EXPECT_FALSE(reg_name.empty())
            << "empty register name for regid " << i;

        EXPECT_TRUE(reg_names.insert(make_pair(reg_name, i)).second)
            << "register name clash for name \'" << reg_name
            << "\' for register ids "
            << reg_names[reg_name] << " and " << i;
    }
}

TEST_F(ocx_core, register_size_not_zero) {
    u64 num_regs = c->num_regs();
    for (u64 i = 0; i < num_regs; ++i) {
        u64 reg_size = c->reg_size(i);
        EXPECT_NE(reg_size, 0)
            << "register " << i << " \'" << c->reg_name(i)
            << "\' has zero size";
    }
}

static bool is_readonly_register(ocx::core* core, u64 regid) {

    if (strcmp(core->arch_family(), "riscv") == 0)
        return (strcmp(core->reg_name(regid), "X0") == 0);

    return false;
}

TEST_F(ocx_core, register_read_write) {

    // the requirements to test reading/writing to the available register is
    // as follows:
    //
    // - if we cannot read the register, we skip it
    // - we attempt to write all ones to the register; if this fails, we
    //   skip the register
    // - after writing all ones, we expect to read at least one set bit,
    //   otherwise we report and error for this register
    // - we expect that we can write all zeroes to the register
    // - after that, we expect that we have not extra bits set and that
    //   at least one bit has been cleared; otherwise we report an error
    // - if we did not find any readable or writable registers, we report
    //   an error

    size_t num_tested = 0;
    std::vector<u8> rbuf;
    std::vector<u8> rbuf_00;
    std::vector<u8> rbuf_ff;
    std::vector<u8> wbuf_00;
    std::vector<u8> wbuf_ff;
    rbuf.resize(4096);
    rbuf_00.resize(4096);
    rbuf_ff.resize(4096);
    wbuf_00.resize(4096);
    wbuf_ff.resize(4096);

    wbuf_00.assign(wbuf_00.size(), 0);
    wbuf_ff.assign(wbuf_ff.size(), 0xff);

    u64 num_regs = c->num_regs();
    for (u64 i = 0; i < num_regs; ++i) {
        u64 regsz = c->reg_size(i);

        // check readability and skip otherwise
        if (!c->read_reg(i, rbuf.data()))
            continue;

        if (is_readonly_register(c, i))
            continue;

        // write all ones, skip on failure
        if (!c->write_reg(i, wbuf_ff.data()))
            continue;

        num_tested++;

        // read back register and check for at least one set bit
        rbuf_ff.assign(rbuf.size(), 0);
        EXPECT_TRUE(c->read_reg(i, rbuf_ff.data()));

        bool found_non_zero = false;
        for (size_t ri = 0; ri < regsz; ++ri) {
            if (rbuf_ff[ri] != 0) {
                found_non_zero = true;
                break;
            }
        }

        EXPECT_TRUE(found_non_zero)
            << "reading only zeroes after writing all ones to register "
            << i << " \'" << c->reg_name(i) << "\'";

        //write all zeroes
        EXPECT_TRUE(c->write_reg(i, wbuf_00.data()));

        // read back register
        rbuf.assign(rbuf_00.size(), 0);
        EXPECT_TRUE(c->read_reg(i, rbuf_00.data()));

        // check for no extra set bits and at least one cleared bit
        bool found_cleared_bit = false;
        for (size_t ri = 0; ri < regsz; ++ri) {
            if (rbuf_00[ri] != rbuf_ff[ri]) {
                if (rbuf_ff[ri] & ~rbuf_00[ri]) {
                    found_cleared_bit = true;
                }

                if (~rbuf_ff[ri] & rbuf_00[ri]) {
                    FAIL()
                        << "found extra set bit after writing all zeroes to "
                        << "register " << i << " \'" << c->reg_name(i) << "\'"
                        << "at " << ri << " old val was " << (int)rbuf_ff[ri]
                        << ", value after writing zeros is " << (int)rbuf_00[ri];
                }
            }
        }

        EXPECT_TRUE(found_cleared_bit)
            << "no cleared bits after writing all zeroes to register  " << i
            << " \'" << c->reg_name(i);
    }

    EXPECT_NE(num_tested, 0) << "found no r/w registers";
}

TEST_F(ocx_core, breakpoint_add_remove) {
    using ::testing::Return;

    void *codebuf = prepare_nop_code(c->page_size(), c->arch_family());
    ASSERT_NE(codebuf, nullptr) <<
            "could not prepare NOP code for " << c->arch_family();

    ON_CALL(env, get_page_ptr_r(0)).WillByDefault(Return((u8 *)codebuf));
    ON_CALL(env, get_page_ptr_w(0)).WillByDefault(Return((u8 *)codebuf));

    ASSERT_TRUE(c->add_breakpoint(0x0));
    ASSERT_TRUE(c->remove_breakpoint(0x0));

    free_nop_code(codebuf);
}

ACTION_P(GetPtr, codebuf, codebuf_sz) {
    return arg0 < codebuf_sz ? (u8*)codebuf + arg0 : nullptr;
}

TEST_F(ocx_core, breakpoint_run) {
    using ::testing::Return;
    using ::testing::_;

    u64 codebuf_sz;
    if (strcmp(c->arch_family(), "X86") == 0)
        codebuf_sz = 0x201000;
    else
        codebuf_sz = c->page_size();

    void *codebuf = prepare_nop_code(codebuf_sz, c->arch_family());
    ASSERT_NE(codebuf, nullptr) <<
            "could not prepare NOP code for " << c->arch_family();

    ON_CALL(env, get_page_ptr_r(_)).WillByDefault(GetPtr(codebuf, codebuf_sz));
    ON_CALL(env, get_page_ptr_w(_)).WillByDefault(GetPtr(codebuf, codebuf_sz));

    c->reset();

    u64 start = 0x100;
    u64 addr = 0x200;
    ASSERT_TRUE(c->add_breakpoint(addr))
        << "failed to set breakpoint";
    ASSERT_TRUE(c->add_breakpoint(addr + 0x100))
        << "failed to set breakpoint";

    u64 pc = c->pc_regid();
    ASSERT_TRUE(c->write_reg(pc, &start))
        << "failed to set PC to  " << std::hex << addr;

    u64 buf = 0;
    ASSERT_TRUE(c->read_reg(pc, &buf)) << "failed to read back PC";
    ASSERT_EQ(start, buf) << "PC has unexpected value";

    // execution should hit the first breakpoint, continue to the
    // second breakpoint and then return from step
    EXPECT_CALL(env, handle_breakpoint(0x200)).WillOnce(Return(false));
    EXPECT_CALL(env, handle_breakpoint(0x300)).WillOnce(Return(true));

    // run in a loop until we hit the second breakpoint; we should not run
    // past that breakpoint
    for (;;) {
        c->step(0x1000);
        ASSERT_TRUE(c->read_reg(pc, &buf)) << "failed to read PC after step";
        if (buf == 0x300)
            break;
        ASSERT_LT(buf, 0x300) << "ran past blocking breakpoint";
    }
    free_nop_code(codebuf);
}

TEST_F(ocx_core, disassemble) {
    using ::testing::Return;
    using ::testing::Invoke;

    void *codebuf = prepare_nop_code(c->page_size(), c->arch_family());
    ASSERT_NE(codebuf, nullptr)
        << "could not prepare NOP code for " << c->arch();

    ON_CALL(env, get_page_ptr_r(0)).WillByDefault(Return((u8 *)codebuf));
    ON_CALL(env, get_page_ptr_w(0)).WillByDefault(Return(nullptr));

    std::vector<char> buf;
    buf.resize(4096);
    buf[0] = '\0';

    u64 bytes = c->disassemble(0, buf.data(), buf.size());

    EXPECT_NE(0, bytes)
        << "disassemble consumed zero bytes";
    EXPECT_NE(0, strnlen(buf.data(), buf.size()))
        << "unexpected empty string returned from disassemble";

    free_nop_code(codebuf);
}

TEST_F(ocx_core, tb_flush) {
    // hard to actually test, but check that we can call the flush
    // methods without crashing the core
    c->tb_flush();
    c->tb_flush_page(0x0, 0x8192);
}

int main(int argc, char** argv) {
    try {
        ::testing::InitGoogleTest(&argc, argv);
        if (argc != 3) {
            std::cerr << "Usage: ocx-test [gtest args] <path_to_ocx_lib> <variant>"
                      << std::endl;
            return -1;
        }

        LIBRARY_PATH = argv[1];
        CORE_VARIANT = argv[2];
        return RUN_ALL_TESTS();
    } catch (const std::exception& err) {
        std::cerr << "Unexpected exception: " << err.what() << std::endl;
        return -2;
    } catch (...) {
        std::cerr << "Unexpected exception" << std::endl;
        return -3;
    }
}
