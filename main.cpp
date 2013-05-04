
#include <signal.h>
#include <unistd.h>
#include "config.h"
#include "logger.h"
#include "tcpproxy.h"
#include "selectloop.h"

class AppMisc
{
public:
    AppMisc() : m_strLogPath("log"), 
        m_strLogApp("tcpproxy"), 
        m_strConfigFile("tcpproxy.json"),
        m_bLogToConsole(true)
    {
        signal(SIGINT, &AppMisc::Sigroutine);
        m_pTcpProxyManager = new TcpProxyManager();
        m_pLoop = new CSelectLoop();
    }
    ~AppMisc()
    {
        delete m_pTcpProxyManager;
        delete m_pLoop;
    }

    static AppMisc& Instance()
    {
        static AppMisc nAppMisc;
        return nAppMisc;
    }
    
    int RunMain(int argc, char* argv[])
    {
        Logger::instance().setPath(m_strLogPath, m_strLogApp);
        Logger::instance().setLevel(Logger::LV_DEBUG);
        ConfigManager::Instance().LoadConfig(m_strConfigFile);
        Logger::instance().setConsole(ConfigManager::Instance().GetLogConsole());
        if (!m_pTcpProxyManager->KickOff(*m_pLoop))
        {
            AILOG_ERROR() << "m_nTcpProxyManager.KickOff error.";
            return -1;
        }
        m_pLoop->RunLoop();
        return 0;
    }

private:
    TcpProxyManager* m_pTcpProxyManager;
    CSelectLoop* m_pLoop;
    std::string m_strLogPath;    //-o
    std::string m_strLogApp;     //-m
    std::string m_strConfigFile; //-c
    bool        m_bLogToConsole;

    void PrintUsageHelp()
    {
        std::cout << "Usage:" << std::endl;
        std::cout << " tcpproxy -o log -m tcpproxy -c tcpproxy.json" << std::endl;
        std::cout << " tcpproxy -c tcpproxy.json" << std::endl;
        std::cout << " tcpproxy" << std::endl;
        std::cout << " tcpproxy -h" << std::endl;
    }
    
    void ParseCmdArg(int argc, char* argv[])
    {
        optind = 1;
        int ch = 0;
        while( (ch = getopt(argc, argv, "o:m:c:hH")) != -1 )
        {
            switch(ch)
            {
            case 'h':
            case 'H':
                PrintUsageHelp();
                exit(0);
                break;
            case 'o':
                m_strLogPath = optarg;
                break;
            case 'm':
                m_strLogApp = optarg;
                break;
            case 'c':
                m_strConfigFile = optarg;
                break;
            default:
                break;
            }
        }
        optind = 1;
        if (!m_strLogPath.empty() && m_strLogApp.empty())
        {
            std::cout << "Usage error.";
            PrintUsageHelp();
            exit(-1);
        }
    }  
    static void Sigroutine(int dunno)
    {
        AILOG_INFO() << "Sigroutine";
        AppMisc::Instance().m_pLoop->Quit();
    }
};

int main(int argc, char* argv[])
{
    AppMisc::Instance().RunMain(argc, argv);
}
