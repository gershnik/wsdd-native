// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#import <Foundation/Foundation.h>

#include <filesystem>

#include <sys/stat.h>

#include <argum/parser.h>

using namespace Argum;

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


auto install() -> bool {
    auto bundle = NSBundle.mainBundle;
    if (auto res = LSRegisterURL((__bridge CFURLRef)bundle.bundleURL, true); res != noErr) {
        fprintf(stderr, "LSRegisterURL failed: %d\n", res);
    }

    std::filesystem::path resourcePath = bundle.resourcePath.UTF8String;
    std::filesystem::path daemonsPath = "/Library/LaunchDaemons";

    if (!installLink(resourcePath / "io.github.gershnik.wsddn.plist", daemonsPath / "io.github.gershnik.wsddn.plist")) 
        return false;
    
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