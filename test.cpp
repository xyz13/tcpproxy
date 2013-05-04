
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#include <set>
#include "config.h"
#include "logger.h"
#include "selectloop.h"

class CTestClientSocket : public CTcpSocket
{
    static const int kPerSize = 10;
    static const int kMaxLoop = 10000;
public:
    CTestClientSocket() : m_nLoopIndex(1), m_nCurrentSendPos(0)
    {
    }
    ~CTestClientSocket()
    {
        AILOG_DEBUG() << "~CTestClientSocket";
    }
    
    virtual void OnConnect()
    {
        ActionConnect();
    }
    
    void ActionConnect()
    {
        std::string strHost;
        int nPort;
        GetLocalAddress(strHost, nPort);
        
        m_strHostPair.resize(500);
        sprintf(&m_strHostPair[0], "%s:%d->%s:%d", strHost.c_str(), nPort,
            GetHost().c_str(), GetPort());
        m_strHostPair.resize(strlen(&m_strHostPair[0]));
        
        AILOG_DEBUG() << m_strHostPair << " OnConnect.";
        
        GenSendData(kPerSize);
        GetLoop()->RegistSocket(this, CSelectLoop::EVT_WRITE);
    }
    
    virtual void OnRead()
    {
        AILOG_DEBUG() << m_strHostPair << " OnRead";
        
        std::string strBuffer;
        strBuffer.resize(1500);
        int nSize = 0;
        if (!Read(&strBuffer[0], strBuffer.size(), nSize))
        {
            if (IsNoneFatal())
            {
                GetLoop()->RegistSocket(this, CSelectLoop::EVT_READ);
            }
            else
            {
                AILOG_DEBUG() << m_strHostPair << " Test error:Read data error." << CTcpSocket::LastErrorMessage();
            }
        }
        if (nSize > 0)
        {
            strBuffer.resize(nSize);
            m_strRecvData.append(strBuffer);
            if (m_strRecvData.size() == m_strSendData.size())
            {
                if (m_strRecvData != m_strSendData)
                {
                    AILOG_DEBUG() << m_strHostPair << "Test error:Send data and Recv data not same!";
                }
                else
                {
                    AILOG_DEBUG() << m_strHostPair << " goto send.";
                    
                    ++m_nLoopIndex;
                    GenSendData(kPerSize * m_nLoopIndex);
                    m_nCurrentSendPos = 0;
                    if (m_nLoopIndex > kMaxLoop)
                        m_nLoopIndex = 1;
                        
                    GetLoop()->RegistSocket(this, CSelectLoop::EVT_WRITE);
                }
            }
            else if (m_strRecvData.size() > m_strSendData.size())
            {
                AILOG_DEBUG() << m_strHostPair << " Test error:Recv more data than Send."
                    << "Send:" << m_strSendData.size() << " Recv:" << m_strRecvData.size();
            }
            else
            {
                GetLoop()->RegistSocket(this, CSelectLoop::EVT_READ);
            }
        }
    }
    virtual void OnWrite()
    {
        AILOG_DEBUG() << m_strHostPair << " OnWrite, pos=" << m_nCurrentSendPos;
        int nSize = m_strSendData.size() - m_nCurrentSendPos;
        int nSendSize = 0;
        if (!Write(&m_strSendData[0] + m_nCurrentSendPos, nSize, nSendSize))
        {
            if (IsNoneFatal())
            {
                GetLoop()->RegistSocket(this, CSelectLoop::EVT_WRITE);
            }
            else
            {
                AILOG_DEBUG() << m_strHostPair << " Test error:Send data error." << CTcpSocket::LastErrorMessage();
            }
        }
        
        if (nSendSize > 0)
        {
            m_nCurrentSendPos += nSendSize;
            if (m_nCurrentSendPos == m_strSendData.size())
            {
                m_nCurrentSendPos = 0;
                m_strRecvData.clear();
                AILOG_DEBUG() << m_strHostPair << " Send data ok, goto Read.";
                GetLoop()->RegistSocket(this, CSelectLoop::EVT_READ);
            }
            else if (m_nCurrentSendPos > m_strSendData.size())
            {
                AILOG_DEBUG() << m_strHostPair << " Send more data than expected.";
            }
            else
            {
                GetLoop()->RegistSocket(this, CSelectLoop::EVT_WRITE);
            }
        }
    }
    virtual void OnError()
    {
        AILOG_ERROR() << m_strHostPair << " OnError";
    }

private:
    void GenSendData(int n)
    {
        AILOG_DEBUG() << "Gen data size:" << n;
        m_strSendData.clear();
        
        char buffer[8];
        sprintf(buffer, "%08d", n);
        m_strSendData.append(buffer);
        
        for (int i = 0; i < n; ++i)
            m_strSendData.push_back('X');
    }
    int         m_nLoopIndex;
    std::string m_strSendData;
    int         m_nCurrentSendPos;
    std::string m_strRecvData;
    std::string  m_strHostPair;
};

class CTestServerSocket;
class CTcpPeerSocket : public CTcpSocket
{
public:
    CTcpPeerSocket(CTestServerSocket* pThis) : 
        m_pThis(pThis),
        m_nContentLen(0),
        m_nSendPos(0)
    {
    }
    ~CTcpPeerSocket()
    {
        AILOG_DEBUG() << "~CTcpPeerSocket";
    }
    virtual void OnRead();
    virtual void OnWrite();
    virtual void OnError();
    
private:
    void Reset()
    {
        m_nSendPos = 0;
        m_nContentLen = 0;
        m_strData.clear();
    }
    CTestServerSocket* m_pThis;
    std::string m_strData;
    int         m_nContentLen;
    int         m_nSendPos;
};

class CTestServerSocket : public CTcpSocket
{
    typedef std::set<CTcpSocket*> StoreSet;
public:
    ~CTestServerSocket()
    {
        AILOG_DEBUG() << "~CTestServerSocket";
    }
    virtual void OnAccept()
    {
        CTcpPeerSocket* pPeer = new CTcpPeerSocket(this);
        if (!Accept(*pPeer))
        {
            delete pPeer;
            AILOG_ERROR() << "Accept conn error!";
        }
        else
        {
            AILOG_DEBUG() << "New conn from:" << pPeer->AddressToString();
            GetLoop()->RegistSocket(pPeer, CSelectLoop::EVT_READ);
            Store(pPeer);
        }
        GetLoop()->RegistSocket(this, CSelectLoop::EVT_ACCEPT);
    }
    virtual void OnError()
    {
        AILOG_ERROR() << "CTestServerSocket::OnError " << LastErrorMessage();
    }
    
    void Store(CTcpSocket* pSocket)
    {
        m_nStoreSet.insert(pSocket);
    }
    
    void UnStore(CTcpSocket* pSocket)
    {
        m_nStoreSet.erase(pSocket);
    }
    
private:
    StoreSet m_nStoreSet;
};

void CTcpPeerSocket::OnRead()
{
    AILOG_DEBUG() << "CTcpPeerSocket::OnRead";
    
    int nSize = 0;
    std::string strBuffer;
    strBuffer.resize(1500);
    if (!Read(&strBuffer[0], 1500, nSize))
    {
        if (IsNoneFatal())
        {
            GetLoop()->RegistSocket(this, CSelectLoop::EVT_READ);
        }
        else
        {
            AILOG_ERROR() << "recv data error:" << CTcpSocket::LastErrorMessage();
            m_pThis->UnStore(this);
            delete this;
        }
    }
    else if (nSize > 0)
    {
        strBuffer.resize(nSize);
        m_strData.append(strBuffer);
        if (m_nContentLen <= 0) //header
        {
            const std::string strLen(strBuffer.substr(0, 8));
            m_nContentLen = atoi(strLen.c_str());
        }
        
        if (m_strData.size() == (m_nContentLen + 8))
        {
            //goto Write
            m_nSendPos = 0;
            GetLoop()->RegistSocket(this, CSelectLoop::EVT_WRITE);
        }
        else if (m_strData.size() > (m_nContentLen + 8))
        {
            AILOG_ERROR() << "Content length:" << m_nContentLen << ", Data len:" << m_strData.size();
            AILOG_ERROR() << "Protocol error:" << m_strData;
        }
        else
        {
            GetLoop()->RegistSocket(this, CSelectLoop::EVT_READ);
        }
    }
    else
    {
        AILOG_INFO() << "Close conn.";
        m_pThis->UnStore(this);
        delete this;
    }
}
void CTcpPeerSocket::OnWrite()
{
    AILOG_DEBUG() << "CTcpPeerSocket::OnWrite";
    
    int nSize = m_strData.size() - m_nSendPos;
    if (!Write(&m_strData[0] + m_nSendPos, nSize, nSize))
    {
        if (IsNoneFatal())
        {
            GetLoop()->RegistSocket(this, CSelectLoop::EVT_WRITE);
        }
        else
        {
            AILOG_ERROR() << "send data error:" << CTcpSocket::LastErrorMessage();
            m_pThis->UnStore(this);
            delete this;
        }
    }
    else
    {
        if (nSize > 0)
        {
            m_nSendPos += nSize;
            if (m_nSendPos == m_strData.size())
            {
                Reset();
                GetLoop()->RegistSocket(this, CSelectLoop::EVT_READ);
            }
            else
            {
                GetLoop()->RegistSocket(this, CSelectLoop::EVT_WRITE);
            }
        }
        else
        {
            AILOG_ERROR() << "send data error:" << CTcpSocket::LastErrorMessage();
            m_pThis->UnStore(this);
            delete this;
        }
    }
}
void CTcpPeerSocket::OnError()
{
    AILOG_ERROR() << "CTcpPeerSocket::OnError";
    m_pThis->UnStore(this);
    delete this;
}

class TestAppMisc
{
    typedef std::set<CTestClientSocket*> ClientSet;
public:
    TestAppMisc() : m_nSvrPort(-1), m_nConnNum(0), m_nPort(-1)
    {
        signal(SIGINT, &TestAppMisc::Sigroutine);
        m_pLoop = new CSelectLoop();
        m_pServer = NULL;
    }
    ~TestAppMisc()
    {
        for (ClientSet::iterator it = m_nClientSet.begin(); it != m_nClientSet.end(); ++it)
        {
            delete *it;
        }
        m_nClientSet.clear();
        delete m_pServer;
        delete m_pLoop;
    }

    static TestAppMisc& Instance()
    {
        static TestAppMisc nTestAppMisc;
        return nTestAppMisc;
    }
    
    int RunMain(int argc, char* argv[])
    {
        ParseCmdArg(argc, argv);
        
        std::string nLogApp;
        if (m_nConnNum > 0 && m_nSvrPort > 0)
        {
            nLogApp = "tcpproxytest";
        }
        else if (m_nConnNum > 0)
        {
            nLogApp = "tcpproxytest.client";
        }
        else
        {
            nLogApp = "tcpproxytest.server";
        }
        Logger::instance().setPath(".", nLogApp.c_str());
        Logger::instance().setLevel(Logger::LV_DEBUG);
        
        if (m_nSvrPort > 0 && !CreateServer())
        {
            return -1;
        }
        if (m_nConnNum > 0 && !CreateClient())
        {
            return -1;
        }
        
        m_pLoop->RunLoop();
        return 0;
    }

private:
    bool CreateServer()
    {
        m_pServer = new CTestServerSocket();
        m_pServer->SetAddress("0.0.0.0", m_nSvrPort);
        if (!m_pServer->Open())
        {
            AILOG_ERROR() << "Open server error";
            return false;
        }
        m_pServer->SetOptNB(true);
        
        if (!m_pServer->Listen())
        {
            AILOG_ERROR() << "Listen server error";
            return false;
        }
        
        AILOG_INFO() << "Create new server:" << m_pServer->AddressToString();
        m_pLoop->RegistSocket(m_pServer, CSelectLoop::EVT_ACCEPT);
        return true;
    }
    bool CreateClient()
    {
        AILOG_INFO() << "CreateClient Target Host:" << m_strHost << ":" << m_nPort;
        for (int i = 0; i < m_nConnNum; ++i)
        {
            CTestClientSocket* pSocket = new CTestClientSocket();
            pSocket->SetAddress(m_strHost, m_nPort);
            if (!pSocket->Open())
            {
                delete pSocket;
                AILOG_ERROR() << "Open client error.";
                return false;
            }
            
            pSocket->SetOptNB(true);
            if (!pSocket->Connect())
            {
                if (CTcpSocket::IsNoneFatal())
                {
                    m_pLoop->RegistSocket(pSocket, CSelectLoop::EVT_CONNECT);
                }
                else
                {
                    AILOG_ERROR() << "CreateClient error." << CTcpSocket::LastErrorMessage();
                    delete pSocket;
                    return false;
                }
            }
            else
            {
                pSocket->ActionConnect();
            }
            
            AILOG_INFO() << "Create new client:" << pSocket->AddressToString();
            m_nClientSet.insert(pSocket);
        }
        return true;
    }
    
private:
    ClientSet    m_nClientSet;
    CTestServerSocket* m_pServer;
    CSelectLoop* m_pLoop;
    int          m_nSvrPort;
    int          m_nConnNum;  //
    std::string  m_strHost;
    int          m_nPort;

    void PrintUsageHelp()
    {
        std::cout << "Usage example:" << std::endl;
        std::cout << " start server and client: tcpproxytest -s 90 -n 10 -d 127.0.0.1 -p 80" << std::endl;
        std::cout << " start server           : tcpproxytest -s 90" << std::endl;
        std::cout << " start client           : tcpproxytest -n 10 -d 127.0.0.1 -p 80" << std::endl;
        std::cout << " display help           : tcpproxytest -h" << std::endl;
    }
    
    void ParseCmdArg(int argc, char* argv[])
    {
        optind = 1;
        int ch = 0;
        while( (ch = getopt(argc, argv, "s:n:d:p:hH")) != -1 )
        {
            switch(ch)
            {
            case 'h':
            case 'H':
                PrintUsageHelp();
                exit(0);
                break;
            case 'n':
                m_nConnNum = atoi(optarg);
                break;
            case 'd':
                m_strHost = optarg;
                break;
            case 'p':
                m_nPort = atoi(optarg);
                break;
            case 's':
                m_nSvrPort = atoi(optarg);
                break;
            default:
                break;
            }
        }
        optind = 1;
        if (m_nConnNum > 0 && (m_strHost.empty() || m_nPort <= 0 || m_nPort >= 65535))
        {
            m_nConnNum = 0;
            if (m_nSvrPort <= 0)
            {
                std::cout << "Usage error.";
                PrintUsageHelp();
                exit(-1);
            }
        }
        
        if (m_nConnNum <= 0 && m_nSvrPort <= 0)
        {
            std::cout << "Usage error.";
            PrintUsageHelp();
            exit(-1);
        }
    }  
    static void Sigroutine(int dunno)
    {
        AILOG_INFO() << "Sigroutine";
        TestAppMisc::Instance().m_pLoop->Quit();
    }
};

int main(int argc, char* argv[])
{
    TestAppMisc::Instance().RunMain(argc, argv);
}
