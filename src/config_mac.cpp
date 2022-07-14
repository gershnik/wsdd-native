// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#if HAVE_APPLE_SAMBA

#include "config.h"

auto Config::detectWinNetInfo(bool useNetbiosHostName) -> std::optional<WinNetInfo> {

    cf_ptr<SCDynamicStoreRef> store = cf_attach(SCDynamicStoreCreate(nullptr, CFSTR("detectWinNetInfo"), nullptr, nullptr));
    if (!store) {
        WSDLOG_ERROR("Unable to create configuration store");
        return std::nullopt;
    }
    cf_ptr<CFPropertyListRef> smb = cf_attach(SCDynamicStoreCopyValue (store.get(), CFSTR("com.apple.smb")));
    if (!smb || CFGetTypeID(smb.get()) != CFDictionaryGetTypeID()) {
        WSDLOG_WARN("SMB info is not present in configuration store");
        return std::nullopt;
    }
     
    auto dict = (CFDictionaryRef)smb.get();
    sys_string_cfstr workgroup = (CFStringRef)CFDictionaryGetValue(dict, CFSTR("Workgroup"));
    sys_string_cfstr hostname = (CFStringRef)CFDictionaryGetValue(dict, CFSTR("NetBIOSName"));
    sys_string_cfstr desc = (CFStringRef)CFDictionaryGetValue(dict, CFSTR("ServerDescription"));
    if (!workgroup.cf_str()) {
        WSDLOG_WARN("Workgroup is missing in configuration store");
    }

    sys_string_cfstr domain;
    cf_ptr<CFPropertyListRef> ad = cf_attach(SCDynamicStoreCopyValue (store.get(), CFSTR("com.apple.opendirectoryd.ActiveDirectory")));
    if (ad && CFGetTypeID(ad.get()) != CFDictionaryGetTypeID()) {
        
        dict = (CFDictionaryRef)ad.get();
        domain = (CFStringRef)CFDictionaryGetValue(dict, CFSTR("DomainNameFlat"));
    }
    
    if (!workgroup.cf_str() && !domain.cf_str()) {
        WSDLOG_WARN("Cannot detect either workgroup or domain from configuration store");
        return std::nullopt;
    }

    WinNetInfo ret;
    sys_string_builder builder;

    if (domain.cf_str()) {
        for(auto c: sys_string_cfstr::utf8_view(domain))
            builder.append(c);
        ret.memberOf.emplace<WindowsDomain>(builder.build());
    } else {
        for(auto c: sys_string_cfstr::utf8_view(workgroup))
            builder.append(c);
        ret.memberOf.emplace<WindowsWorkgroup>(builder.build());
    }
    
    if (useNetbiosHostName && hostname.cf_str()) {
        for(auto c: sys_string_cfstr::utf8_view(hostname))
            builder.append(c);
        ret.hostName = builder.build();
    } else {
        if (useNetbiosHostName)
            ret.hostName = m_simpleHostName.to_upper();
        else 
            ret.hostName = m_simpleHostName;
    }
    
    if (desc.cf_str()) {
        for(auto c: sys_string_cfstr::utf8_view(desc))
            builder.append(c);
        ret.hostDescription = builder.build();
    } else {
        ret.hostDescription = m_simpleHostName;
    }

    return ret;
}

#endif
