/* Copyright (c) 2018 - present, VE Software Inc. All rights reserved
 *
 * This source code is licensed under Apache 2.0 License
 *  (found in the LICENSE.Apache file in the root directory)
 */

#include "base/Base.h"
#include <gtest/gtest.h>
#include <fstream>
#include "process/ProcessUtils.h"
#include "fs/FileUtils.h"

namespace nebula {

TEST(ProcessUtils, getExePath) {
    {
        auto result = ProcessUtils::getExePath();
        ASSERT_TRUE(result.ok()) << result.status();
        ASSERT_NE(std::string::npos, result.value().find("process_test")) << result.value();
    }
    {
        auto result = ProcessUtils::getExePath(1);  // systemd
        ASSERT_FALSE(result.ok());
    }
}


TEST(ProcessUtils, getExeCWD) {
    {
        auto result = ProcessUtils::getExeCWD();
        ASSERT_TRUE(result.ok()) << result.status();
        char buffer[PATH_MAX];
        ::getcwd(buffer, sizeof(buffer));
        ASSERT_EQ(buffer, result.value());
    }
    {
        auto result = ProcessUtils::getExeCWD(1);   // systemd
        ASSERT_FALSE(result.ok());
    }
}


TEST(ProcessUtils, isPidAvailable) {
    {
        auto status = ProcessUtils::isPidAvailable(::getpid());
        ASSERT_FALSE(status.ok());
    }
    {
        auto status = ProcessUtils::isPidAvailable(0);  // idle/swap
        ASSERT_FALSE(status.ok());
    }
    {
        auto status = ProcessUtils::isPidAvailable(1);  // systemd
        ASSERT_FALSE(status.ok());
    }
    {
        // pid file which contains pid of current process
        auto pidFile = "/tmp/process_test.pid";
        auto status = ProcessUtils::makePidFile(pidFile);
        ASSERT_TRUE(status.ok()) << status;
        status = ProcessUtils::isPidAvailable(pidFile);
        ASSERT_FALSE(status.ok());
        ::unlink(pidFile);
    }
    {
        // pid file not exist
        auto pidFile = "/tmp/definitely-not-exist.pid";
        auto status = ProcessUtils::isPidAvailable(pidFile);
        ASSERT_TRUE(status.ok()) << status;
    }
    {
        // pid file not readable
        auto pidFile = "/proc/sys/vm/compact_memory";
        auto status = ProcessUtils::isPidAvailable(pidFile);
        ASSERT_FALSE(status.ok());
    }
    {
        // choose an available pid
        auto genPid = [] () {
            auto max = ProcessUtils::maxPid();
            while (true) {
                uint32_t next = static_cast<uint32_t>(folly::Random::rand64());
                next %= max;
                if (::kill(next, 0) == -1 && errno == ESRCH) {
                    return next;
                }
            }
        };
        auto pidFile = "/tmp/process_test.pid";
        auto status = ProcessUtils::makePidFile(pidFile, genPid());
        ASSERT_TRUE(status.ok()) << status;
        // there are chances that the chosen pid was occupied already,
        // but the chances are negligible, so be it.
        status = ProcessUtils::isPidAvailable(pidFile);
        ASSERT_TRUE(status.ok()) << status;
        ::unlink(pidFile);
    }
}


TEST(ProcessUtils, getProcessName) {
    {
        auto result = ProcessUtils::getProcessName();
        ASSERT_TRUE(result.ok()) << result.status();
        ASSERT_NE(std::string::npos, result.value().find("process_test")) << result.value();
    }
    {
        auto result = ProcessUtils::getProcessName(1);
        ASSERT_TRUE(result.ok()) << result.status();
        ASSERT_EQ("systemd", result.value()) << result.value();
    }
}


TEST(ProcessUtils, runCommand) {
    auto status1 = ProcessUtils::runCommand("echo $HOME");
    ASSERT_TRUE(status1.ok()) << status1.status();
    EXPECT_EQ(std::string(getenv("HOME")),
              folly::rtrimWhitespace(status1.value()).toString());

    // Try large output
    auto status2 = ProcessUtils::runCommand("cat /etc/profile");
    ASSERT_TRUE(status2.ok()) << status2.status();
    std::ifstream is("/etc/profile", std::ios::ate);
    auto size = is.tellg();
    EXPECT_EQ(size, status2.value().size());
    std::string buf(size, '\0');
    is.seekg(0);
    is.read(&buf[0], size);
    EXPECT_EQ(buf, status2.value());
}

}   // namespace nebula