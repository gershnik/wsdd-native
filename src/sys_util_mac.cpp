// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#if HAVE_APPLE_USER_CREATION

#include "sys_util.h"

static const CFArrayCallBacks g_arrayCallbacks = {
    .version = 0,
    .retain = [](CFAllocatorRef, const void * value) { return CFRetain(value); },
    .release = [](CFAllocatorRef, const void * value) { CFRelease(value); },
    .copyDescription = nullptr,
    .equal = CFEqual
};

static const CFDictionaryKeyCallBacks g_dictKeyCallbacks = {
    .version = 0,
    .retain = [](CFAllocatorRef, const void * value) { return CFRetain(value); },
    .release = [](CFAllocatorRef, const void * value) { CFRelease(value); },
    .copyDescription = nullptr,
    .equal = CFEqual,
    .hash = CFHash
};

static const CFDictionaryValueCallBacks g_dictValueCallbacks = {
    .version = 0,
    .retain = [](CFAllocatorRef, const void * value) { return CFRetain(value); },
    .release = [](CFAllocatorRef, const void * value) { CFRelease(value); },
    .copyDescription = nullptr,
    .equal = CFEqual
};

[[noreturn]] static void throwCFError(const cf_ptr<CFErrorRef> & err) {

    sys_string_cfstr desc(CFErrorCopyDescription(err.get()), sysstr::handle_retain::no);
    sys_string_cfstr::char_access access(desc);
    throw std::runtime_error(access.c_str());
}

template<size_t N>
static auto makeArray(CFTypeRef (&values)[N]) -> cf_ptr<CFArrayRef> {
    return cf_attach(CFArrayCreate(nullptr, values, N, &g_arrayCallbacks));
}

template<class... Args>
static auto makeArray(Args ...args) -> cf_ptr<CFArrayRef> {
    CFTypeRef values[] = {args...}; 
    return cf_attach(CFArrayCreate(nullptr, values, sizeof...(args), &g_arrayCallbacks));
}

template<size_t N>
static auto makeDictionary(const std::pair<CFTypeRef, CFTypeRef> (&entries)[N]) -> cf_ptr<CFDictionaryRef> {
    CFTypeRef keys[N];
    CFTypeRef values[N]; 
    for (size_t i = 0; i < N; ++i) {
        keys[i] = entries[i].first;
        values[i] = entries[i].second;
    }
    return cf_attach(CFDictionaryCreate(nullptr, keys, values, N, &g_dictKeyCallbacks, &g_dictValueCallbacks));
}

template<class... Args>
static auto makeDictionary(Args ...args) -> cf_ptr<CFDictionaryRef> {
    CFTypeRef keys[] = {std::get<0>(args)...};
    CFTypeRef values[] = {std::get<1>(args)...}; 
    return cf_attach(CFDictionaryCreate(nullptr, keys, values, sizeof...(args), &g_dictKeyCallbacks, &g_dictValueCallbacks));
}

static auto IsODError(const cf_ptr<CFErrorRef> & err, ODFrameworkErrors value) {
    return sys_string_cfstr(CFErrorGetDomain(err.get())) == kODErrorDomainFramework &&
           CFErrorGetCode(err.get()) == value;
}

static auto getStringAttribute(const cf_ptr<ODRecordRef> & record, CFStringRef name) -> std::optional<sys_string_cfstr> {

    cf_ptr<CFErrorRef> err;
    auto valArray = cf_attach(ODRecordCopyValues(record.get(), name, err.get_output_param()));
    if (!valArray)
        return std::nullopt;
    if (CFArrayGetCount(valArray.get()) != 1)
        return std::nullopt;
    auto value = (CFStringRef)CFArrayGetValueAtIndex(valArray.get(), 0);
    if (!value)
        return sys_string_cfstr();
    if (CFGetTypeID(value) != CFStringGetTypeID())
        return std::nullopt;
    return sys_string_cfstr(value);
}

static void setAttribute(const cf_ptr<ODRecordRef> & record,
                         ODAttributeType attributeType, const cf_ptr<CFArrayRef> & value) {
    
    cf_ptr<CFErrorRef> err;
//    auto existing = cf_attach(ODRecordCopyValues(record.get(), attributeType, err.get_output_param()));
//    if (existing)
//        return false;
    if (!ODRecordSetValue(record.get(), attributeType, value.get(), err.get_output_param()))
        throwCFError(err);
    if (!ODRecordSynchronize(record.get(), err.get_output_param()))
        throwCFError(err);
//    return true;
}

template<class T>
static auto parseId(const sys_string_cfstr & str, T & val) -> bool {
    sys_string_cfstr::char_access access(str);
    const char * first = access.c_str(), * last = first + strlen(first);
    auto res = std::from_chars(first, last, val);
    return res.ec == std::errc() && res.ptr == last;
}

template<class T>
static auto getFirstFreeIdGreaterThan(const std::set<T> & ids, T start) -> T {

    T next = start + 1;
    for (T val: ids) {
        if (val > next)
            break;
        next = val + 1;
    }
    return next;
} 

static auto getAvailableIdGreaterThan(const cf_ptr<ODNodeRef> & localNode, 
                                      ODRecordType recordType,
                                      ODAttributeType attributeType,
                                      unsigned startId) -> unsigned {

    cf_ptr<CFErrorRef> err;

    auto attrNames = makeArray(attributeType);
    sys_string_cfstr strId = std::to_string(startId);
    auto query = cf_attach(ODQueryCreateWithNode(nullptr, localNode.get(), recordType,
                                                 attributeType, kODMatchGreaterThan, strId.cf_str(),
                                                 attrNames.get(), std::numeric_limits<CFIndex>::max(), err.get_output_param()));
    if (!query)
        throwCFError(err);

    auto results = cf_attach(ODQueryCopyResults(query.get(), false, err.get_output_param()));
    if (!results)
        throwCFError(err);

    std::set<unsigned> ids;
    for (CFIndex i = 0, count = CFArrayGetCount(results.get()); i < count; ++i) {
        auto record = cf_retain((ODRecordRef)CFArrayGetValueAtIndex(results.get(), i));

        auto idValue = getStringAttribute(record, attributeType);
        if (!idValue)
            continue;
        
        unsigned val;
        if (!parseId(*idValue, val))
            continue;
        ids.insert(val);
    }
    return getFirstFreeIdGreaterThan(ids, startId);
}

static auto createRecordWithUniqueId(const cf_ptr<ODNodeRef> & localNode, 
                                     const sys_string_cfstr & name,
                                     ODRecordType recordType,
                                     ODAttributeType uniqueIdAttribute,
                                     unsigned startId) -> std::pair<cf_ptr<ODRecordRef>, unsigned> {

    cf_ptr<CFErrorRef> err;
    cf_ptr<ODRecordRef> record;
    unsigned idValue;

    auto attrToFetch = makeArray(kODAttributeTypeAllAttributes);
    for ( ; ; ) {

        record = cf_attach(ODNodeCopyRecord(localNode.get(), recordType, name.cf_str(), 
                                            attrToFetch.get(), err.get_output_param()));

        if (!record) {
            
            idValue = getAvailableIdGreaterThan(localNode, recordType, uniqueIdAttribute, startId - 1);
            auto strId = sys_string_cfstr(std::to_string(idValue));
            auto attrs = makeDictionary({{uniqueIdAttribute, makeArray(strId.cf_str()).get()}});
            record = cf_attach(ODNodeCreateRecord(localNode.get(), recordType,
                                                  name.cf_str(), attrs.get(), err.get_output_param()));
            if (!record) {
                if (IsODError(err, kODErrorRecordAlreadyExists))
                    continue;
                throwCFError(err);
            }
        } else {
            
            auto maybeIdStr = getStringAttribute(record, uniqueIdAttribute);
            if (maybeIdStr) {
                if (!parseId(*maybeIdStr, idValue))
                    throw std::runtime_error("Invalid identifier attribute value");
                break;
            }
            idValue = getAvailableIdGreaterThan(localNode, recordType, uniqueIdAttribute, startId - 1);
            auto strId = sys_string_cfstr(std::to_string(idValue));
            if (!ODRecordSetValue(record.get(), uniqueIdAttribute, strId.cf_str(), err.get_output_param()))
                throwCFError(err);
            if (!ODRecordSynchronize(record.get(), err.get_output_param()))
                throwCFError(err);
        }
        break;
    }
    return {record, idValue};
}

auto Identity::createDaemonUser(const sys_string & name) -> Identity {

    cf_ptr<CFErrorRef> err;
    
    cf_ptr<ODNodeRef> localNode = cf_attach(ODNodeCreateWithNodeType(nullptr, kODSessionDefault, kODNodeTypeLocalNodes, 
                                                                     err.get_output_param()));
    if (!localNode)
        throwCFError(err);

    sys_string_cfstr cfName(name.c_str());
    auto [group, gid] = createRecordWithUniqueId(localNode, cfName, kODRecordTypeGroups, kODAttributeTypePrimaryGroupID, 700);
    auto [user,  uid] = createRecordWithUniqueId(localNode, cfName, kODRecordTypeUsers,  kODAttributeTypeUniqueID,       700);
    
    setAttribute(user, kODAttributeTypePrimaryGroupID,     makeArray(sys_string_cfstr(std::to_string(gid)).cf_str()));
    setAttribute(user, kODAttributeTypeUserShell,          makeArray(CFSTR("/usr/bin/false")));
    setAttribute(user, kODAttributeTypePassword,           makeArray(CFSTR("*")));
    setAttribute(user, kODAttributeTypeNFSHomeDirectory,   makeArray(CFSTR("/var/empty")));
    setAttribute(user, kODAttributeTypeFullName,           makeArray(CFSTR("WS-Discovery Daemon")));
    
    setAttribute(group, kODAttributeTypeGroupMembership,   makeArray(cfName.cf_str()));
    setAttribute(group, kODAttributeTypePassword,          makeArray(CFSTR("*")));
    setAttribute(group, kODAttributeTypeFullName,          makeArray(CFSTR("WS-Discovery Daemon")));

    return Identity(uid, gid);
}

#endif
