// Copyright (c) 2022, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_XML_WRAPPER_H_INCLUDED
#define HEADER_XML_WRAPPER_H_INCLUDED


inline const xmlChar * asXml(const char8_t * str) {
    return reinterpret_cast<const xmlChar *>(str);
}

inline xmlChar * asXml(char8_t * str) {
    return reinterpret_cast<xmlChar *>(str);
}

inline const char8_t * asNative(const xmlChar * str) {
    return reinterpret_cast<const char8_t *>(str);
}

inline char8_t * asNative(xmlChar * str) {
    return reinterpret_cast<char8_t *>(str);
}

inline const char8_t * xml_str(const sys_string & str) {
    return reinterpret_cast<const char8_t *>(str.c_str());
}

inline const char8_t * xml_str(const std::string & str) {
    return reinterpret_cast<const char8_t *>(str.c_str());
}

class XmlCharBuffer {
private:
    struct Free {
        void operator()(void * ptr) const {
            if (ptr)
                xmlFree(ptr);
        } 
    };
    using MemPtr = std::unique_ptr<char8_t, Free>;
public:
    XmlCharBuffer(xmlChar * mem, int size): 
        m_buf(asNative(mem)), 
        m_size(size_t(size)) {
    }

    size_t size() const {
        return m_size;
    }

    char8_t * data() {
        return m_buf.get();
    }
    const char8_t * data() const {
        return m_buf.get();
    }

private:
    MemPtr m_buf;
    size_t m_size;
};

struct XmlParserInit {
    XmlParserInit() {
        xmlInitParser();

    //libxml marked xmlThrDefXxx variants deprecated without providing any sane
    //alternative to suppress its idiotic default logging to stderr
    //We will keep using these for now.
    WSDDN_IGNORE_DEPRECATED_BEGIN
        xmlSetGenericErrorFunc(nullptr, XmlParserInit::errorFunc);
        xmlThrDefSetGenericErrorFunc(nullptr, XmlParserInit::errorFunc);
        
        xmlSetStructuredErrorFunc(nullptr, XmlParserInit::structuredErrorFunc);
        xmlThrDefSetStructuredErrorFunc(nullptr, XmlParserInit::structuredErrorFunc);
    WSDDN_IGNORE_DEPRECATED_END
    
    }
    ~XmlParserInit() {
        xmlCleanupParser();
    }
    XmlParserInit(const XmlParserInit &) = delete;
    XmlParserInit & operator=(const XmlParserInit &) = delete;

private:
    static void errorFunc(void * /*ctx*/, const char * /*msg*/, ...) {

    }
    //Older versions of LibXml have this callback without const
    static void structuredErrorFunc(void * /*userData*/, const xmlError * /*error*/) {

    }
    static void structuredErrorFunc(void * /*userData*/, xmlError * /*error*/) {

    }
};

class XmlException : public std::runtime_error {
public:
    XmlException(const xmlError * err):
        std::runtime_error(err->message ? err->message : fmt::format("error: {0}, domain: {1}", err->code, err->domain)),
        m_domain(err->domain),
        m_code(err->code) {

    }
    XmlException(xmlParserErrors err): 
        std::runtime_error(fmt::format("XML parser error: {0}", int(err))),
        m_domain(XML_FROM_PARSER),
        m_code(err){

    }

    static void raiseFromLastErrorIfPresent() {
        auto err = xmlGetLastError();
        if (err) {
            auto ex = XmlException(err);
            xmlResetLastError();
            throw ex;
        }
    }

    [[noreturn]] static void raiseFromLastError() {
        raiseFromLastErrorIfPresent();
        throw std::runtime_error("xml failure with no error set");
    }

    int domain() const {
        return m_domain;
    }
    int code() const {
        return m_code;
    }
private:
    int	m_domain;
    int	m_code;
};

template<class T, class Derived>
class XmlHandle : protected T
{
protected:
    using Wrapped = T;
public:
    void operator delete(void *) noexcept
    {}
    
    XmlHandle() = delete;
    XmlHandle(const XmlHandle &) = delete;
    XmlHandle & operator=(const XmlHandle &) = delete;
    
    static Derived * from(T * obj) noexcept
        { return static_cast<Derived *>(obj); }

    friend T * c_ptr(const XmlHandle<T, Derived> * obj) noexcept {
        return const_cast<T *>(static_cast<const T *>(obj)); 
    }
    
protected:
    ~XmlHandle() noexcept
    {}
};


class XmlNs : public XmlHandle<xmlNs, XmlNs> {
public:
    ~XmlNs() {
        xmlFreeNs(this);
    }

    const char8_t * href() const {
        return asNative(Wrapped::href);
    }
};


class XmlAttr;
class XmlDoc;

class XmlNode : public XmlHandle<xmlNode, XmlNode> {
public:
    ~XmlNode() {
        xmlFreeNode(this);
    }

    static std::unique_ptr<XmlNode> create(const XmlNs * ns, const char8_t * name) {
        if (auto ret = from(xmlNewNode(c_ptr(ns), asXml(name))))
            return std::unique_ptr<XmlNode>(ret);
        XmlException::raiseFromLastError();
    }

    static std::unique_ptr<XmlNode> createText(const char8_t * text) {
        if (auto ret = from(xmlNewText(asXml(text))))
            return std::unique_ptr<XmlNode>(ret);
        XmlException::raiseFromLastError();
    }
    
    XmlNs & newNs(const char8_t * href, const char8_t * prefix) {
        if (auto ret = XmlNs::from(xmlNewNs(this, asXml(href), asXml(prefix))))
            return *ret;
        XmlException::raiseFromLastError();
    }

    void setNs(XmlNs * ns) {
        xmlSetNs(this, c_ptr(ns));
    }

    XmlNode & newChild(const XmlNs * ns, const char8_t * name, const char8_t * content = nullptr) {
        if (auto ret = from(xmlNewChild(this, c_ptr(ns), asXml(name), asXml(content))))
            return *ret;
        XmlException::raiseFromLastError();
    }

    XmlNode & newTextChild(const XmlNs * ns, const char8_t * name, const char8_t * content) {
        if (auto ret = from(xmlNewTextChild(this, c_ptr(ns), asXml(name), asXml(content))))
            return *ret;
        XmlException::raiseFromLastError();
    }

    XmlNode & addChild(XmlNode & child) {
        if (auto ret = from(xmlAddChild(this, &child)))
            return *ret;
        XmlException::raiseFromLastError();
    }

    XmlAttr & newAttr(const XmlNs * ns, const char8_t * name, const char8_t * value);
    
    XmlDoc * document() const;

    xmlElementType type() const {
        return Wrapped::type;
    }
    
    XmlAttr * firstProperty() const;
    
    XmlNode * firstChild() const
        { return XmlNode::from(this->children); }
    XmlNode * nextSibling() const
        { return XmlNode::from(this->next); }
    XmlNode * parent() const
        { return XmlNode::from(c_ptr(this)->parent); }

    sys_string getContent() const {
        auto chars = asNative(xmlNodeGetContent(this));
        if (!chars)
            XmlException::raiseFromLastErrorIfPresent();
        std::unique_ptr<char8_t, decltype(xmlFree)> holder(chars, xmlFree);
        return sys_string(chars);
    }
    
    void setContent(const char8_t * content);
};

class XmlAttr : public XmlHandle<xmlAttr, XmlAttr> {
public:
    ~XmlAttr() {
        xmlFreeProp(this);
    }
    
    XmlNode * firstChild() const
        { return XmlNode::from(this->children); }
    XmlAttr * nextSibling() const
        { return XmlAttr::from(this->next); }
    XmlNode * parent() const
        { return XmlNode::from(c_ptr(this)->parent); }
};

inline XmlAttr & XmlNode::newAttr(const XmlNs * ns, const char8_t * name, const char8_t * value) {
    if (auto ret = XmlAttr::from(xmlNewNsProp(this, c_ptr(ns), asXml(name), asXml(value))))
        return *ret;
    XmlException::raiseFromLastError();
}


class XmlDoc : public XmlHandle<xmlDoc, XmlDoc> {
public:
    ~XmlDoc() {
        xmlFreeDoc(this); 
    }

    static std::unique_ptr<XmlDoc> parseMemory(const void * ptr, int size) {
        if (auto ret = from(xmlParseMemory((const char *)ptr, size)))
            return std::unique_ptr<XmlDoc>(ret);
        XmlException::raiseFromLastError();
    }

    static std::unique_ptr<XmlDoc> create(const char8_t * version) {
        if (auto ret = from(xmlNewDoc(asXml(version))))
            return std::unique_ptr<XmlDoc>(ret);
        XmlException::raiseFromLastError();
    }

    XmlCharBuffer dump(bool format = 0) {
        xmlChar * mem = 0;
        int size;
        xmlDocDumpFormatMemoryEnc(this, &mem, &size, "utf-8", format);
        if (!mem)
            XmlException::raiseFromLastError();
        return XmlCharBuffer(mem, size);
    }

    XmlNode * asNode() {
        return XmlNode::from(this->children);
    }
    XmlNode * getRootElement() {
        return XmlNode::from(xmlDocGetRootElement(this));
    }
    std::unique_ptr<XmlNode> setRootElement(std::unique_ptr<XmlNode> root) {
        auto ret = XmlNode::from(xmlDocSetRootElement(this, c_ptr(root.get())));
        if (xmlDocGetRootElement(this) != c_ptr(root.get()))
            XmlException::raiseFromLastError();
        root.release();
        return std::unique_ptr<XmlNode>(ret);
    }
    
    XmlNs * searchNs(XmlNode & node, const char8_t * nameSpace) const {
        auto ret = XmlNs::from(xmlSearchNs(c_ptr(this), c_ptr(&node), asXml(nameSpace)));
        if (!ret) {
            XmlException::raiseFromLastErrorIfPresent();
        }
        return ret;
    }
    
    
    std::unique_ptr<XmlNode> copyNode(XmlNode & node) {
        auto ret = XmlNode::from(xmlDocCopyNode(c_ptr(&node), c_ptr(this), 1));
        if (!ret) {
            XmlException::raiseFromLastError();
        }
        return std::unique_ptr<XmlNode>(ret);
    }
};

inline XmlDoc * XmlNode::document() const {
    return XmlDoc::from(this->doc);
}

inline XmlAttr * XmlNode::firstProperty() const {
    return XmlAttr::from(this->properties);
}

inline void XmlNode::setContent(const char8_t * content) {
    auto encoded = asNative(xmlEncodeSpecialChars(c_ptr(document()), asXml(content)));
    if (!encoded)
        XmlException::raiseFromLastError();
    std::unique_ptr<char8_t, decltype(xmlFree)> holder(encoded, xmlFree);
    xmlNodeSetContent(this, asXml(encoded));
    XmlException::raiseFromLastErrorIfPresent();
}

class XmlNodeset : public XmlHandle<xmlNodeSet, XmlNodeset> {
public:
    ~XmlNodeset() {
        xmlXPathFreeNodeSet(this);
    }

    bool empty() const {
        return !(this->nodeNr) || !(this->nodeTab);
    }

    size_t size() const {
        if (!this->nodeTab)
            return 0;
        return this->nodeNr;
    }

    XmlNode * operator[](size_t idx) {
        return XmlNode::from(this->nodeTab[idx]);
    }
};

class XPathObject : public XmlHandle<xmlXPathObject, XPathObject> {
public:
    ~XPathObject() {
        xmlXPathFreeObject(this);
    }

    XmlNodeset * nodeset() const {
        return XmlNodeset::from(this->nodesetval);
    }

    XmlNode * firstNode() {
        auto nodes = nodeset();
        if (!nodes || nodes->empty())
            return nullptr;
        return (*nodes)[0];
    }

    const char8_t * stringval() const {
        return asNative(Wrapped::stringval);
    }
};

class XPathContext : public XmlHandle<xmlXPathContext, XPathContext> {
public:
    ~XPathContext() {
        xmlXPathFreeContext(this); 
    }
    static std::unique_ptr<XPathContext> create(XmlDoc & doc) {
        if (auto ret = from(xmlXPathNewContext(c_ptr(&doc))))
            return std::unique_ptr<XPathContext>(ret);
        XmlException::raiseFromLastError();
    }

    void registerNs(const char8_t * prefix, const char8_t * uri) {
        if (xmlXPathRegisterNs(this, asXml(prefix), asXml(uri)) != 0)
            XmlException::raiseFromLastError();
    }

    void setContextNode(XmlNode & node) {
        if (xmlXPathSetContextNode(c_ptr(&node), this) != 0)
            XmlException::raiseFromLastError();
    }

    std::unique_ptr<XPathObject> eval(const char8_t * expr) {
        if (auto ret = XPathObject::from(xmlXPathEval(asXml(expr), this)))
            return std::unique_ptr<XPathObject>(ret);
        XmlException::raiseFromLastError();
    }
};

class XmlParserContext : public XmlHandle<xmlParserCtxt, XmlParserContext> {
public:
    ~XmlParserContext() noexcept {
        if (this->myDoc)
            xmlFreeDoc(this->myDoc);
        xmlFreeParserCtxt(this);
    }

    static std::unique_ptr<XmlParserContext> createPush(const uint8_t * chunk, int size, const char * filename = nullptr) {

        if (auto ret = from(xmlCreatePushParserCtxt(nullptr, nullptr, (const char *)chunk, size, filename)))
            return std::unique_ptr<XmlParserContext>(ret);
        XmlException::raiseFromLastError();
    }

    static std::unique_ptr<XmlParserContext> createPush(const char8_t * encoding = nullptr, const char * filename = nullptr) {

        auto ret = std::unique_ptr<XmlParserContext>(from(xmlCreatePushParserCtxt(nullptr, nullptr, nullptr, 0, filename)));
        if (!ret)
            XmlException::raiseFromLastError(); 
        if (encoding) {
            ret->encoding = xmlStrdup(asXml(encoding));
            if (!ret->encoding)
                XmlException::raiseFromLastError(); 
        }
        return ret;
    }

    void parseChunk(const uint8_t * chunk, int size, bool last) {
        if (auto err = xmlParserErrors(xmlParseChunk(this, (const char *)chunk, size, last)))
            throw XmlException(err);
    }

    std::unique_ptr<XmlDoc> extractDoc() {
        std::unique_ptr<XmlDoc> ret(XmlDoc::from(this->myDoc));
        this->myDoc = nullptr;
        return ret;
    }

    XmlDoc * doc() const {
        return XmlDoc::from(this->myDoc);
    }

    bool wellFormed() const {
        return Wrapped::wellFormed;
    }
};

#endif
