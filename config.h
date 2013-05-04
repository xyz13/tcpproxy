#ifndef __TCPPROXY_CONFIG_H__
#define __TCPPROXY_CONFIG_H__

#include <string>
#include <map>
#include <vector>
#include <json/json.h>

class ProxyItem
{
public:
    std::string h1;
    int         p1;
    std::string h2;
    int         p2;
    std::string proto;
};
typedef std::vector<ProxyItem> ProxyItemPool;

class ConfigManager
{
public:    
    static ConfigManager& Instance();
    
    bool LoadConfig(const std::string& strFile = "");
    const ProxyItemPool& GetProxyItemPool() const { return m_nProxyItemPool; }
    
    bool GetLogConsole() const { return m_bLogToConsole; }
private:
    bool ParseProxyPool(const Json::Value& nValue);
    
    bool m_bLogToConsole;
    ProxyItemPool m_nProxyItemPool;
};

#endif
