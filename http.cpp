
#include "http.h"
#include "string.h"
#include <cstdio>
#include <cstdlib>

HttpHeader::HttpHeader()
{
}

struct Code2String
{
    int m_nCode;
    std::string m_pString;
};

static const std::string kNullString = "";

const std::string& HttpHeader::GetRespCodeString(int nCode)
{
    static Code2String nCode2String[] = {
        {100, "Continue"},
        {101, "Switching Protocols"},
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {203, "Non-Authoritative Information"},
        {204, "No Content"},
        {205, "Reset Content"},
        {206, "Partial Content"},
        {300, "Multiple Choices"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {303, "See Other"},
        {304, "Not Modified"},
        {305, "Use Proxy"},
        {306, "(Unused)"},
        {307, "Temporary Redirect"},
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {402, "Payment Required"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {406, "Not Acceptable"},
        {407, "Proxy Authentication Required"},
        {408, "Request Timeout"},
        {409, "Conflict"},
        {410, "Gone"},
        {411, "Length Required"},
        {412, "Precondition Failed"},
        {413, "Request Entity Too Large"},
        {414, "Request-URI Too Long"},
        {415, "Unsupported Media Type"},
        {416, "Requested Range Not Satisfiable"},
        {417, "Expectation Failed"},
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"},
        {504, "Gateway Timeout"},
        {505, "HTTP Version Not Supported"}        
    };
    
    static int kSize = sizeof(nCode2String) / sizeof(nCode2String[0]);
    for (int i = 0; i < kSize; ++i)
    {
        if (nCode == nCode2String[i].m_nCode)
            return nCode2String[i].m_pString;
    }
    
    return kNullString;
}

static bool FindLine(const char* pBuffer, int nDataLen,
    int& nColonPos, int& nRNPos, int& nConsoleLen)
{
    nColonPos = nRNPos = -1;
    nConsoleLen = 0;
    
    int i = 0;
    for (; i < nDataLen && nRNPos == -1; ++i)
    {
        switch (pBuffer[i])
        {
        case ':':
            nColonPos = i;
            break;
        case '\r':
            if (nDataLen > (i + 1) && pBuffer[i + 1] == '\n')
            {
                nRNPos = i;
                ++i;
            }
            break;
        case '\n':
            nRNPos = i;
            break;
        default:
            break;
        }
    }
    
    if (nRNPos == -1)
        return false;
    
    nConsoleLen = i;
    return true;
}

static std::string Int2String(int n)
{
    char buffer[32];
    sprintf(buffer, "%d", n);
    return buffer;
}

static int Strint2Int(const char* s)
{
    return atoi(s);
}

static int Strint2Int(const std::string& s)
{
    return Strint2Int(s.c_str());
}

typedef std::vector<std::string> StringArray;
static int SplitString(const char* str, std::size_t s, char c, StringArray& n)
{
    const char* pos = str;
    const char* epos = pos + s;
    
    while (pos < epos && *pos == c) ++pos;
    const char* pos1 = pos;
    
    for ( ; pos < epos; ++pos)
    {
        if (*pos == c)
        {
            if (pos > pos1)
            {
                const std::string ns(pos1, pos - pos1);
                n.push_back(ns);
            }
            pos1 = pos + 1;
        }
    }
    if (pos > pos1)
    {
        const std::string ns(pos1, pos - pos1);
        n.push_back(ns);
    }
    return n.size();
}

static int SplitString(const char* str, char c, StringArray& n)
{
    return SplitString(str, strlen(str), c, n);
}

static int SplitString(const std::string& str, char c, StringArray& n)
{
    return SplitString(str.c_str(), str.size(), c, n);
}

static std::string TrimString(const std::string& str, char c)
{
    const std::size_t pos = str.find_first_not_of(c);
    const std::size_t pos1 = str.find_last_not_of(c);
    if (pos == std::string::npos)
        return "";
    return str.substr(pos, pos1 - pos + 1);
}

void HttpHeader::Parse(const char* pBuffer, int nDataLen, int& nHeaderLen)
{
    nHeaderLen = 0;
    
    bool bMayResp = strncasecmp(pBuffer, "HTTP", 4) == 0;
    bool bMayReq = (nDataLen > 3 && (strncasecmp(pBuffer, "GET", 3) == 0 ||
                    strncasecmp(pBuffer, "PUT", 3) == 0)) ||
                   (nDataLen > 4 && (strncasecmp(pBuffer, "HEAD", 4) == 0 ||
                    strncasecmp(pBuffer, "POST", 4) == 0)) ||
                   (nDataLen > 5 && strncasecmp(pBuffer, "TRACE", 5) == 0) ||
                   (nDataLen > 6 && strncasecmp(pBuffer, "DELETE", 6) == 0) ||
                   (nDataLen > 7 && (strncasecmp(pBuffer, "OPTIONS", 7) == 0 ||
                    strncasecmp(pBuffer, "CONNECT", 7))) == 0;
    
    if (!bMayReq && !bMayResp)
        return;
    
    int nPos = 0;
    int nConsoleLen = 0;
    int nColonPos = 0;
    int nRNPos = 0;
    
    if (!FindLine(pBuffer, nDataLen, nColonPos, nRNPos, nConsoleLen))
    {
        return;
    }
    
    StringArray nArray;
    SplitString(pBuffer, nRNPos, ' ', nArray);
    if (nArray.size() != 3)
        return;
    
    if (bMayResp)
    {
        StringArray nVerArray;
        SplitString(nArray[0], '/', nVerArray);
        if (nVerArray.size() != 2)
            return;
        
        m_bRequest = false;
        m_strVersion = nVerArray[1];
        m_nRespCode = Strint2Int(nArray[1]);
    }
    else
    {
        StringArray nVerArray;
        SplitString(nArray[2], '/', nVerArray);
        if (nVerArray.size() != 2)
            return;
        
        m_bRequest = true;
        m_strVersion = nVerArray[1];
        m_strMethod = nArray[0];
        m_strUrl = nArray[1];
    }
    
    int nItemLen = 0;
    if (ParseHeaderItem(pBuffer + nConsoleLen, nDataLen - nConsoleLen, nItemLen))
    {
        nHeaderLen = nConsoleLen + nItemLen;
    }
}

bool HttpHeader::ParseHeaderItem(const char* pBuffer, int nDataLen, int& nHeaderLen)
{
    int nConsoleLen = 0;
    int nColonPos = 0;
    int nRNPos = 0;
    
    const char* pPos = pBuffer;
    while (true)
    {
        if (!FindLine(pPos, nDataLen, nColonPos, nRNPos, nConsoleLen))
        {
            return false;
        }
        if (nRNPos == 0) //\r\n\r\n meet
        {
            nHeaderLen += nConsoleLen;
            break;
        }
        
        if (nColonPos == -1) //none : found
        {
            return false;
        }
        
        if (nRNPos <= (nColonPos + 1)) //empty value
        {
            return false;
        }
        
        const std::string strName(pPos, nColonPos);
        const std::string strValue(pPos + nColonPos + 1, nRNPos - nColonPos - 1);
        
        HttpHeaderItem nItem;
        nItem.m_strName = TrimString(strName, ' ');
        nItem.m_strValue = TrimString(strValue, ' ');
        m_nItemArray.push_back(nItem);
                
        nHeaderLen += nConsoleLen;
        pPos += nConsoleLen;
        nDataLen -= nConsoleLen;
    }
    
    return true;
}

void HttpHeader::SetHeader(const std::string& strName, const std::string& strValue)
{
    int i = GetHeaderIndex(strName);
    if (i >= 0)
    {
        m_nItemArray[i].m_strValue = strValue;
    }
    else
    {
        HttpHeaderItem n;
        n.m_strName = strName;
        n.m_strValue = strValue;
        m_nItemArray.push_back(n);
    }
}

void HttpHeader::DeleteHeader(const std::string& strName)
{
    int i = GetHeaderIndex(strName);
    if (i >= 0)
    {
        m_nItemArray.erase(m_nItemArray.begin() + i);
    }
}

std::string& HttpHeader::Encode(std::string& strData)
{
    if (m_bRequest)
    {
        strData = m_strMethod + " " + (m_strUrl.empty()? "/" : m_strUrl) 
            + " HTTP/" + m_strVersion + "\r\n";
    }
    else
    {
        strData = std::string("HTTP/") + m_strVersion + " " + 
            Int2String(m_nRespCode) + " " + GetRespCodeString(m_nRespCode) + "\r\n";
    }
    
    for (int i = 0; i < m_nItemArray.size(); ++i)
    {
        const HttpHeaderItem& n = m_nItemArray[i];
        strData += n.m_strName + ": " + n.m_strValue + "\r\n";
    }
    strData += "\r\n";
    
    return strData;   
}

const std::string& HttpHeader::GetHeader(const std::string& strName) const
{
    int i = GetHeaderIndex(strName);
    if (i >= 0)
    {
        return m_nItemArray[i].m_strValue;
    }
    
    return kNullString;
}

int HttpHeader::GetHeaderIndex(const std::string& strName) const
{
    for (int i = 0; i < m_nItemArray.size(); ++i)
    {
        if (strncasecmp(m_nItemArray[i].m_strName.c_str(), 
            strName.c_str(), m_nItemArray[i].m_strName.size()) == 0)
            return i;
    }
    return -1;
}
