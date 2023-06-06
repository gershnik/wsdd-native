// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#import <Foundation/Foundation.h>

#include <filesystem>

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <sys/stat.h>

#include <argum/parser.h>
#include <sys_string/sys_string.h>

using namespace Argum;
using namespace sysstr;

auto installLink(const std::filesystem::path & src, const std::filesystem::path & dst) -> bool {
    std::error_code ec;
    create_directories(dst.parent_path(), ec);
    if (ec) {
        fprintf(stderr, "Unable to create directories of %s: %s\n", dst.c_str(), ec.message().c_str());
        return false;
    }
    (void)remove(dst, ec);
    create_symlink(src, dst, ec);
    if (ec) {
        fprintf(stderr, "Cannot link %s to %s: %s\n", dst.c_str(), src.c_str(), ec.message().c_str());
        return false;
    }
    return true;
}

auto installCopy(const std::filesystem::path & src, const std::filesystem::path & dst) -> bool {
    std::error_code ec;
    create_directories(dst.parent_path(), ec);
    if (ec) {
        fprintf(stderr, "Unable to create directories of %s: %s\n", dst.c_str(), ec.message().c_str());
        return false;
    }
    (void)remove(dst, ec);
    copy_file(src, dst, ec);
    if (ec) {
        fprintf(stderr, "Cannot copy %s to %s: %s\n", src.c_str(), dst.c_str(), ec.message().c_str());
        return false;
    }
    return true;
}

auto install() -> bool {
    auto bundle = NSBundle.mainBundle;
    fprintf(stderr, "Bundle url: %s\n", bundle.bundleURL.absoluteString.UTF8String);
    if (auto res = LSRegisterURL((__bridge CFURLRef)bundle.bundleURL, true); res != noErr) {
        fprintf(stderr, "LSRegisterURL failed: %d\n", res);
    }

    std::filesystem::path resourcePath = bundle.resourcePath.UTF8String;
    std::filesystem::path root = "/";

    std::filesystem::path items[] = {
        "usr/local/bin/wsddn", 
        "usr/local/share/man/man8/wsddn.8.gz",
        "etc/wsddn.conf.sample",
        "Library/LaunchDaemons/io.github.gershnik.wsddn.plist"
    };
    
    for(auto & item: items) {
        auto src = resourcePath / item;
        auto dest = root / item;
        if (!installLink(src, dest)) 
            return false;
    }

    auto conf = root / "etc/wsddn.conf";
    bool copySample = false;
    std::error_code ec;
    std::filesystem::file_status confStat = symlink_status(conf, ec);
    if (confStat.type() != std::filesystem::file_type::regular) {
        copySample = true;
    }
    
    if (copySample) {
        if (!installCopy(root / "etc/wsddn.conf.sample", conf))
            return false;
    }
    
    if (int res = system("/bin/launchctl load -w \"/Library/LaunchDaemons/io.github.gershnik.wsddn.plist\""); res != 0) {
        fprintf(stderr, "loading daemon failed: %d\n", res);
        return false;
    }

    return true;
}

int main(int argc, char ** argv) {
    umask(S_IWGRP | S_IWOTH);

    const char * progname = getprogname();
    bool doInstall = false;

    Parser parser;
    parser.add(
        Option("--install", "-i").
        help("install the daemon"). 
        handler([&]() {
            doInstall = true;
    }));

    auto result = parser.parse(argc, argv);
    if (auto error = result.error()) {
        auto msg = error->message();
        fprintf(stderr, "%.*s\n", int(msg.size()), msg.data());
        fprintf(stderr, "%s\n", parser.formatUsage(progname).c_str());
        return EXIT_FAILURE;
    }

    if (doInstall) {
        if (!install()) 
            return EXIT_FAILURE;
    }
}