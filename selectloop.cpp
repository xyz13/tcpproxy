
#include "selectloop.h"
#if defined(WIN32)
#include <winsock.h>
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#endif 
#include <map>
#include "logger.h"

CSelectLoop::CSelectLoop() : m_bKeepRunning(false), 
    m_bHasQuit(false), m_pWaitupServer(NULL), m_pWaitupClient(NULL)
{
}

CSelectLoop::~CSelectLoop()
{
    Cleanup();
    AILOG_INFO() << "CSelectLoop::~CSelectLoop.";
}

void CSelectLoop::Cleanup()
{
    delete m_pWaitupServer;
    delete m_pWaitupClient;
    m_pWaitupServer = NULL;
    m_pWaitupClient = NULL;
}

bool CSelectLoop::RunLoop()
{
    AILOG_INFO() << "CSelectLoop::RunLoop enter.";
    
    m_pWaitupServer = new CWaitupServerSocket();
    m_pWaitupClient = new CWaitupServerSocket();

    if (!CreateWaitup())
    {
        Cleanup();
        return false;
    }

    m_bKeepRunning = true;
    m_bHasQuit = false;
    
    if (m_nWorkingMap.empty())
        AILOG_INFO() << "none socket found.";
    
    typedef std::vector<SocketData> SocketDataCache;
    SocketDataCache nSocketDataCache;
    
    while (m_bKeepRunning && !m_nWorkingMap.empty())
    {
        nSocketDataCache.clear();
        
        int nMaxFd = -1;
        fd_set rfds, wfds, efds;
        FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);
        for (Fd2SocketDataMap::iterator it = m_nWorkingMap.begin(); it != m_nWorkingMap.end(); ++it)
        {
            SocketData& nItem = it->second;
            if (nItem.m_nEvt & (EVT_ACCEPT | EVT_READ))
                FD_SET(nItem.m_pSocket->GetFd(), &rfds);
            if (nItem.m_nEvt & (EVT_WRITE | EVT_CONNECT))
                FD_SET(nItem.m_pSocket->GetFd(), &wfds);
            FD_SET(nItem.m_pSocket->GetFd(), &efds);
            nMaxFd = (nItem.m_pSocket->GetFd() > nMaxFd)? nItem.m_pSocket->GetFd(): nMaxFd;
            nSocketDataCache.push_back(nItem);
        }
        
        int nRet = EINTR_HANDLER(select(nMaxFd + 1, &rfds, &wfds, &efds, NULL));
        if (nRet == -1)
        {
            m_bHasQuit = true;
            AILOG_ERROR() << "select error:" << CTcpSocket::LastErrorMessage();
            Cleanup();
            return false;
        }
        
        //消息分发
        for (int i = 0; i < nSocketDataCache.size(); ++i)
        {
            SocketData& nItem = nSocketDataCache[i];
            CTcpSocket* pSocket = nItem.m_pSocket;
            unsigned int nEvts = nItem.m_nEvt;
            
            if (FD_ISSET(pSocket->GetFd(), &efds))
            {
                AILOG_ERROR() << "fd exeption!:" 
                    << "fd=" << pSocket->GetFd() << ",error=" << CTcpSocket::LastErrorMessage();
                UnRegistSocket(pSocket);
                pSocket->OnError();
            }
            else
            {
                if (FD_ISSET(pSocket->GetFd(), &rfds))
                {
                    UnRegistSocket(pSocket);
                    if (nEvts & EVT_ACCEPT)
                    {
                        pSocket->OnAccept();
                    }
                    else
                    {
                        pSocket->OnRead();
                    }
                }
                
                if (FD_ISSET(pSocket->GetFd(), &wfds))
                {
                    UnRegistSocket(pSocket);
                    if (nEvts & EVT_CONNECT)
                    {
                        int error = 0;
                        socklen_t len = sizeof(error); 
                        if((getsockopt(pSocket->GetFd(), SOL_SOCKET,
                            SO_ERROR, (char*)&error, &len)) != 0 && error != 0)
                        {
                            AILOG_ERROR() << "[getsockopt]juge connect error:" 
                                << CTcpSocket::LastErrorMessage();
                            pSocket->OnError();
                        }
                        else
                        {
                            pSocket->OnConnect();
                        }
                    }
                    else if (nEvts & EVT_WRITE)
                    {
                        pSocket->OnWrite();
                    }
                }
            }
        }
    }
    
    Cleanup();
    AILOG_INFO() << "CSelectLoop::RunLoop leave.";
    m_bHasQuit = true;
    return true;
}

void CSelectLoop::CWaitupServerSocket::OnRead()
{
    AILOG_DEBUG() << "Waitup OnRead 1";
    char c;
    int nSize = 0;
    while (Read(&c, 1, nSize) && nSize > 0);
    AILOG_DEBUG() << "Waitup OnRead 2";
    if (IsNoneFatal())
    {
        AILOG_DEBUG() << "Waitup OnRead: RegistSocket for read";
        GetLoop()->RegistSocket(this, CSelectLoop::EVT_READ);
    }
    else //quit the loop
    {
        AILOG_ERROR() << "quit the loop,waitup socket error:" << LastErrorMessage();
        GetLoop()->Quit();
    }
}

bool CSelectLoop::CreateWaitup()
{
    if (!CTcpSocket::OpenSocketPair(*m_pWaitupClient, *m_pWaitupServer))
    {
        AILOG_ERROR() << "CreateWaitup[OpenSocketPair] error," << CTcpSocket::LastErrorMessage();
        return false;
    }
    
    m_pWaitupServer->SetOptNB(true);
    RegistSocket(m_pWaitupServer, EVT_READ);
    
    return true;
}

void CSelectLoop::Waitup()
{
    AILOG_DEBUG() << "CSelectLoop::Waitup";
    int nSize = 0;
    if (!m_pWaitupClient->Write("W", 1, nSize))
    {
        AILOG_ERROR() << "Send Waitup msg error:" << CTcpSocket::LastErrorMessage();
    }
}

void CSelectLoop::RegistSocket(CTcpSocket* pSocket, unsigned int nEvt)
{    
    if (nEvt == 0)
        return;
        
    //AILOG_INFO() << "RegistSocket" << pSocket->GetFd() <<"," << pSocket->m_bRegist;
    //already regist,now modify it
    Fd2SocketDataMap::iterator it = m_nWorkingMap.find(pSocket->GetFd());
    if (it != m_nWorkingMap.end())
    {
        it->second.m_nEvt = nEvt;
    }
    else
    {
        pSocket->m_bRegist = true;
        pSocket->m_nLoop = this;
        m_nWorkingMap.insert(std::make_pair(pSocket->GetFd(), SocketData(pSocket, nEvt)));
    }
}

void CSelectLoop::UnRegistSocket(CTcpSocket* pSocket)
{
    //AILOG_INFO() << "UnRegistSocket" << pSocket->GetFd() <<"," << this << "," << pSocket->m_bRegist;
    m_nWorkingMap.erase(pSocket->GetFd());
    pSocket->m_bRegist = false;
}

unsigned int CSelectLoop::GetRegistEvts(CTcpSocket* pSocket)
{
    Fd2SocketDataMap::iterator it = m_nWorkingMap.find(pSocket->GetFd());
    if (it != m_nWorkingMap.end())
    {
        return it->second.m_nEvt;
    }
    return 0;
}

void CSelectLoop::Quit()
{
    AILOG_INFO() << "Quit";
    if (m_bKeepRunning)
    {
        AILOG_INFO() << "Need Waitup the loop";
        m_bKeepRunning = false;
        Waitup();
    }
    AILOG_INFO() << "Quit leave";
}

void CSelectLoop::QuitUntil()
{
    AILOG_INFO() << "QuitUntil";
    Quit();
    
    #if !defined(WIN32)
    #define Sleep(ms) usleep((ms) * 1000)
    #endif
    
    while(m_bHasQuit) Sleep(50);
    AILOG_INFO() << "QuitUntil leave";
}
