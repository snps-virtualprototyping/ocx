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
    MOCK_METHOD1(transport, response(const transaction&));
    MOCK_METHOD2(signal, void(u64, bool));
    MOCK_METHOD2(broadcast_syscall, void(int, void*));
    MOCK_METHOD0(get_time_ps, u64());
    MOCK_METHOD2(notify, void(u64, u64));
    MOCK_METHOD1(cancel, void(u64));
    MOCK_METHOD1(hint, void(ocx::hint_kind));
    MOCK_METHOD1(handle_breakpoint, bool(u64));
    MOCK_METHOD4(handle_watchpoint, bool(u64, u64, u64, bool));
    MOCK_METHOD1(handle_begin_basic_block, void(u64));
    MOCK_METHOD1(get_param, const char*(const char*));
};

void* prepare_nop_code(size_t code_size, const std::string& arch) {
    void* result = valloc(code_size);
    const char* nop;
    size_t nopsz;

    if (arch.find("ARMv8") == 0) {
        nop = "\x1f\x20\x03\xd5";
        nopsz = 4;
    } else if (arch == "openrisc") {
        nop = "\x00\x00\x00\x15";
        nopsz = 4;
    } else {
        std::cerr << "unknown architecture " << arch << std::endl;
        free(result);
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

    void* handle = dlopen(LIBRARY_PATH, RTLD_LOCAL | RTLD_NOW | RTLD_NOLOAD);
    EXPECT_EQ(handle, nullptr)
        << "unloading shared library " << LIBRARY_PATH
        << " seems to have failed";
}

TEST(ocx_basic, instantiate_core) {
    corelib cl(LIBRARY_PATH);
    mock_env env;
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

    c->set_id(1, 1);
    cl.delete_core(c);
}

TEST(ocx_basic, mismatched_version) {
    corelib cl(LIBRARY_PATH);
    mock_env env;
    ocx::core* c = cl.create_core(env, CORE_VARIANT, 0);
    ASSERT_EQ(c, nullptr)
        << "library returned core instance despite API version mismatch";
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

TEST_F(ocx_core, register_read_write) {
    size_t num_tested = 0;
    std::vector<u8> rbuf;
    rbuf.resize(4096);
    u8 wbuf_ff[4096];
    u8 wbuf_00[4096];

    memset(wbuf_ff, 0xff, sizeof(wbuf_ff));
    memset(wbuf_00, 0x00, sizeof(wbuf_00));

    u64 num_regs = c->num_regs();
    for (u64 i = 0; i < num_regs; ++i) {
        u64 regsz = c->reg_size(i);
        if (!c->read_reg(i, rbuf.data()))
            continue;

        if (!c->write_reg(i, wbuf_ff))
            continue;

        num_tested++;

        rbuf.assign(rbuf.size(), 0);
        EXPECT_TRUE(c->read_reg(i, rbuf.data()));

        bool found_non_zero = false;
        for (size_t ri = 0; ri < regsz; ++ri) {
            if (rbuf[ri] != 0) {
                found_non_zero = true;
                break;
            }
        }

        EXPECT_TRUE(found_non_zero)
            << "reading only zeroes after writing all ones to register "
            << i << " \'" << c->reg_name(i) << "\'";

        EXPECT_TRUE(c->write_reg(i, wbuf_00));

        rbuf.assign(rbuf.size(), 0);
        EXPECT_TRUE(c->read_reg(i, rbuf.data()));

        for (size_t ri = 0; ri < regsz; ++ri) {
            EXPECT_EQ(0, rbuf[ri])
                << "unexpected non-zero value read from register " << i
                << " \'" << c->reg_name(i) << "\' in byte " << ri;
        }
    }

    EXPECT_NE(num_tested, 0) << "found no r/w registers";
}

TEST_F(ocx_core, breakpoint_add_remove) {
    ASSERT_TRUE(c->add_breakpoint(0x0));
    ASSERT_TRUE(c->remove_breakpoint(0x0));
}

TEST_F(ocx_core, breakpoint_run) {
    using ::testing::Return;

    void *codebuf = prepare_nop_code(c->page_size(), c->arch_family());
    ASSERT_NE(codebuf, nullptr) <<
            "could not prepare NOP code for " << c->arch_family();

    u64 addr = 0x200;
    ASSERT_TRUE(c->add_breakpoint(addr))
        << "failed to set breakpoint";
    ASSERT_TRUE(c->add_breakpoint(addr + 0x100))
        << "failed to set breakpoint";

    u64 pc = c->pc_regid();
    ASSERT_TRUE(c->write_reg(pc, &addr))
        << "failed to set PC to  " << std::hex << addr;

    u64 buf = 0;
    ASSERT_TRUE(c->read_reg(pc, &buf)) << "failed to read back PC";
    ASSERT_EQ(addr, buf) << "PC has unexpected value";

    ON_CALL(env, get_page_ptr_r(0)).WillByDefault(Return((u8 *)codebuf));
    ON_CALL(env, get_page_ptr_w(0)).WillByDefault(Return(nullptr));

    EXPECT_CALL(env, handle_breakpoint(0x200)).WillOnce(Return(false));
    EXPECT_CALL(env, handle_breakpoint(0x300)).WillOnce(Return(true));
    c->step(100);
    free(codebuf);
}

TEST_F(ocx_core, disassemble) {
    using ::testing::Return;
    using ::testing::Invoke;

    void *codebuf = prepare_nop_code(c->page_size(), c->arch_family());
    ASSERT_NE(codebuf, nullptr)
        << "could not prepare NOP code for " << c->arch();

    std::vector<char> buf;
    buf.resize(4096);
    buf[0] = '\0';

    u64 bytes = c->disassemble(codebuf, c->page_size(), buf.data(), buf.size());

    EXPECT_NE(0, bytes)
        << "disassemble consumed zero bytes";
    EXPECT_NE(0, strnlen(buf.data(), buf.size()))
        << "unexpected empty string returned from disassemble";

    free(codebuf);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (argc != 3) {
        std::cout << "Usage: ocx-test [gtest args] <path_to_ocx_lib> <variant>"
                  << std::endl;
        return -1;
    }

    LIBRARY_PATH = argv[1];
    CORE_VARIANT = argv[2];
    return RUN_ALL_TESTS();
}
