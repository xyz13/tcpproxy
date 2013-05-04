
#include "config.h"
#include "logger.h"
#include <cstdio>

const char* gConfigFile = "tcpproxy.json";

ConfigManager& ConfigManager::Instance()
{
    static ConfigManager nConfigManager;
    return nConfigManager;
}

bool ConfigManager::LoadConfig(const std::string& strFile)
{
    const std::string strConfFile = strFile.empty() ? gConfigFile : strFile;
    FILE* fp = fopen(strConfFile.c_str(), "rt");
    if (!fp)
    {
        AILOG_FATAL() << "Open config file:" << strConfFile << " error!";
        return false;
    }
    
    std::string strData;
    char buffer[1024 + 1];
    while (!feof(fp))
    {
        int nSize = fread(buffer, 1, 1024, fp);
        if (nSize < 0)
        {
            fclose(fp);
            AILOG_FATAL() << "Read config file:" << strConfFile << " error!";
            return false;
        }
        buffer[nSize] = '\0';
        strData += buffer;
    }
    fclose(fp);
    
    Json::Value nValue;
    Json::Reader nReader;
    if (!nReader.parse(strData, nValue))
    {
        AILOG_FATAL() << "parse config data error:" << strData;
        return false;
    }
    
    bool bRet = false;
    do
    {
        m_bLogToConsole = nValue["log_console"].asBool();
        
        if (!ParseProxyPool(nValue))
        {
            break;
        }
        bRet = true;
    } while(false);
    
    return bRet;
}

/*
//h1:p1->h2:p2
"proxy_pool": [
    { "h1":"127.0.0.1", "p1":80, "h2":"www.google.com", "p2":80 }
]
*/
bool ConfigManager::ParseProxyPool(const Json::Value& nValue)
{
    if (!nValue.isMember("proxy_pool"))
    {
        AILOG_FATAL() << "none proxy_pool found!";
        return false;
    }
    
    const Json::Value nProxyPool = nValue["proxy_pool"];
    for (int i = 0; i < nProxyPool.size(); ++i)
    {
        const Json::Value& n = nProxyPool[i];
        ProxyItem nItem;
        nItem.h1 = n["h1"].asString();
        nItem.p1 = n["p1"].asInt();
        nItem.h2 = n["h2"].asString();
        nItem.p2 = n["p2"].asInt();
        if (n.isMember("proto"))
        {
            nItem.proto = n["proto"].asString();
        }
        m_nProxyItemPool.push_back(nItem);
        AILOG_INFO() << nItem.h1 << ":" << nItem.p1 << "<->" 
            << nItem.h2 << ":" << nItem.p2;
    }
    
    return true;
}
