// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#import <Foundation/Foundation.h>

#include <stdio.h>

int main(int argc, char ** argv) {
    auto bundle = NSBundle.mainBundle;
    auto res = LSRegisterURL((__bridge CFURLRef)bundle.bundleURL, false);
    if (res != noErr) {
        fprintf(stderr, "LSRegisterURL failed: %d", res);
    }
}
