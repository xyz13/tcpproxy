#ifndef __TCP_PROXY_HTTP_H__
#define __TCP_PROXY_HTTP_H__

#include <vector>
#include <string>

struct HttpHeaderItem
{
    std::string m_strName;
    std::string m_strValue;
};
typedef std::vector<HttpHeaderItem> HttpHeaderItemArray;

class HttpHeader
{
public:
    HttpHeader();
    void Parse(const char* pBuffer, int nDataLen, int& nHeaderLen);
    void SetRequest(bool n) { m_bRequest = n; }
    void SetVersion(const std::string& n) { m_strVersion = n; }
    void SetMethod(const std::string& n) { m_strMethod = n; }
    void SetUrl(const std::string& n) { m_strUrl = n; }
    void SetRespCode(int n) { m_nRespCode = n; }
    void SetHeader(const std::string& strName, const std::string& strValue);
    void DeleteHeader(const std::string& strName);
    std::string& Encode(std::string& strData);
    
    bool  IsRequest() const { return m_bRequest; }
    const std::string& GetVersion() const { return m_strVersion; }
    const std::string& GetMethod() const { return m_strMethod; }
    const int GetRespCode() const { return m_nRespCode; }
    const std::string& GetHeader(const std::string& strName) const;
    
    static const std::string& GetRespCodeString(int nCode);

private:
    bool ParseHeaderItem(const char* pBuffer, int nDataLen, int& nHeaderLen);
    int GetHeaderIndex(const std::string& strName) const;
    
    bool        m_bRequest;
    std::string m_strVersion;
    std::string m_strMethod;
    std::string m_strUrl;
    int         m_nRespCode;
    HttpHeaderItemArray m_nItemArray;
};

#endif
