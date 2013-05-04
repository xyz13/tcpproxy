
#include "tcpproxy.h"
#include "logger.h"
#include "http.h"

TcpProxyServerSocket::~TcpProxyServerSocket()
{
    AILOG_INFO() << "TcpProxyServerSocket::~TcpProxyServerSocket()";
}

void TcpProxyServerSocket::OnAccept()
{
    TcpProxyPeerSocket* pSocket = new TcpProxyPeerSocket(m_nProxy, true);
    do
    {
        if (!Accept(*pSocket))
        {
            AILOG_ERROR() << AddressToString() << " OnAccept error:" << LastErrorMessage();
            break;
        }
        
        if (!pSocket->SetOptNB(true))
        {
            AILOG_ERROR() << AddressToString() << " OnAccept error:" << LastErrorMessage();
            break;
        }
        
        AILOG_INFO() << AddressToString() << " get new conn from " << pSocket->AddressToString();
        TcpProxyPeerSocket* pSocketBrother = new TcpProxyPeerSocket(m_nProxy, false);
        pSocketBrother->SetAddress(m_nProxy.GetProxyItem().h2, m_nProxy.GetProxyItem().p2);
        pSocket->m_pBrother = pSocketBrother;
        pSocketBrother->m_pBrother = pSocket;
        
        if (!pSocketBrother->Open())
        {
            AILOG_ERROR() << pSocketBrother->AddressToString() << " Open error:" << LastErrorMessage();
            break;
        }
        
        if (!pSocketBrother->SetOptNB(true))
        {
            AILOG_ERROR() << pSocketBrother->AddressToString() << " SetOptNB error:" << LastErrorMessage();
            break;
        }
        
        AILOG_INFO() << "start connect to " << pSocketBrother->AddressToString();
        if (!pSocketBrother->Connect())
        {
            if (IsNoneFatal())
            {
                AILOG_INFO() << "waiting connection finished: " << pSocketBrother->AddressToString();
                GetLoop()->RegistSocket(pSocketBrother, CSelectLoop::EVT_CONNECT);
            }
            else
            {
                AILOG_ERROR() << pSocketBrother->AddressToString() << " Connect error:" << LastErrorMessage();
                break;
            }
        }
        else //has connected
        {
            AILOG_INFO() << "connected to " << pSocketBrother->AddressToString();
            GetLoop()->RegistSocket(pSocketBrother, CSelectLoop::EVT_READ);
            GetLoop()->RegistSocket(pSocket, CSelectLoop::EVT_READ);
        }
        
        m_nProxy.Store(pSocket);
        pSocket = NULL;
        
    } while(false);
    
    if (pSocket) delete pSocket;

    GetLoop()->RegistSocket(this, CSelectLoop::EVT_ACCEPT);
}

TcpProxyPeerSocket::~TcpProxyPeerSocket()
{
    AILOG_INFO() << "TcpProxyPeerSocket::~TcpProxyPeerSocket() master=" << m_bMaster;
    if (m_bMaster)
    {
        delete m_pBrother;
    }
}

//the master has been store!!
void TcpProxyPeerSocket::OnConnect()
{
    AILOG_INFO() << "connected to " << AddressToString();
    
    GetLoop()->RegistSocket(m_pBrother, CSelectLoop::EVT_READ);
    GetLoop()->RegistSocket(this, CSelectLoop::EVT_READ);
}

unsigned int TcpProxyPeerSocket::GetMarkEvt()
{
    unsigned int x = 0;
    if (m_nReadBuffer.Empty()) x |= CSelectLoop::EVT_READ;
    if (!m_nWriteBuffer.Empty()) x |= CSelectLoop::EVT_WRITE;
    return x;
}

void TcpProxyPeerSocket::OnRead()
{
    //AILOG_INFO() << "TcpProxyPeerSocket::OnRead" << m_nReadBuffer.DataLen() << "\t" << m_nReadBuffer.VistPos();
    if (!m_nReadBuffer.Empty()) //do nothing if there has some data stored!
    {
        AILOG_FATAL() << "Should not go here!";
    }
    
    m_nReadBuffer.Reset();
    if (!Read(m_nReadBuffer.GetBuffer(), m_nReadBuffer.GetBufferLen(), m_nReadBuffer.DataLen()))
    {
        if (IsNoneFatal())
        {
            GetLoop()->RegistSocket(this, GetMarkEvt());
        }
        else
        {
            AILOG_ERROR() << AddressToString() << " OnRead[Read] error " << LastErrorMessage();
            m_nProxy.UnStore(GetMaster());
            delete GetMaster();
        }       
    }
    else if (m_nReadBuffer.DataLen() == 0) //none data recv, close the socket
    {
        AILOG_INFO() << AddressToString() << " close";
        m_nProxy.UnStore(GetMaster());
        delete GetMaster();
    }
    else
    {
        ProcessProto(m_nReadBuffer);
        if (m_pBrother->m_nWriteBuffer.Empty())
        {
            m_pBrother->m_nWriteBuffer.Reset();
            m_pBrother->m_nWriteBuffer.Swap(m_nReadBuffer);
            GetLoop()->RegistSocket(m_pBrother, m_pBrother->GetMarkEvt());
        }
        
        GetLoop()->RegistSocket(this, GetMarkEvt());
    }
}

void TcpProxyPeerSocket::ProcessProto(Buffer& nBuffer)
{
    const ProxyItem& nProxyItem = m_nProxy.GetProxyItem();
    if (nProxyItem.proto == "http")
    {
        HttpHeader nHttpHeader;
        int nHeaderLen = 0;
        nHttpHeader.Parse((char*)nBuffer.GetBuffer(), nBuffer.DataLen(), nHeaderLen);
        
        if (nHeaderLen > 0)
        {
            const std::string& strHost = nHttpHeader.GetHeader("Host");
            
            if (strHost == nProxyItem.h1)
            {
                AILOG_DEBUG() << "Modify host from:" << nProxyItem.h1 << " to " << nProxyItem.h2;
                
                //modify it to h2
                nHttpHeader.SetHeader("Host", nProxyItem.h2);
                std::string strHeaderData;
                nHttpHeader.Encode(strHeaderData);
                
                int nTotalLen = strHeaderData.size() + nBuffer.DataLen() - nHeaderLen;
                if (nTotalLen > nBuffer.m_pBuffer->size())
                {
                    nBuffer.m_pBuffer->resize(nTotalLen);
                }
                
                if (nTotalLen < nBuffer.DataLen())
                {
                    memcpy(nBuffer.GetBuffer(), &strHeaderData[0], strHeaderData.size());
                    const char* p0 = (char*)nBuffer.GetBuffer() + nHeaderLen;
                    const char* p0e = p0 + nBuffer.DataLen();
                    char* p1 = (char*)nBuffer.GetBuffer() + strHeaderData.size();
                    for ( ; p0 < p0e; ++p0, ++p1) *p1 = *p0; //move to front
                    nBuffer.DataLen() = nTotalLen;
                }
                else if (nTotalLen > nBuffer.DataLen())
                {
                    const char* p0 = (char*)nBuffer.GetBuffer() + nBuffer.DataLen() - 1;
                    const char* p0e = p0 - (nBuffer.DataLen() - nHeaderLen);
                    char* p1 = (char*)nBuffer.GetBuffer() + nTotalLen - 1;
                    for ( ; p0 >= p0e; --p0, --p1) *p1 = *p0; //move to back
                    memcpy(nBuffer.GetBuffer(), &strHeaderData[0], strHeaderData.size());
                    nBuffer.DataLen() = nTotalLen;
                }
                else
                {
                    memcpy(nBuffer.GetBuffer(), &strHeaderData[0], nHeaderLen);
                }
            }
        }
    }
}

void TcpProxyPeerSocket::OnWrite()
{
    //AILOG_INFO() << "TcpProxyPeerSocket::OnWrite" << m_nWriteBuffer.DataLen() << "\t" << m_nWriteBuffer.VistPos();
    if (m_nWriteBuffer.Empty())
    {
        AILOG_FATAL() << "Should not go here!";
    }
    
    int nLen = 0;
    if (!Write(m_nWriteBuffer.GetVistBuffer(), m_nWriteBuffer.LeftLen(), nLen))
    {
        if (IsNoneFatal())
        {
            GetLoop()->RegistSocket(this, GetMarkEvt());
        }
        else
        {
            AILOG_ERROR() << AddressToString() << " OnWrite[Write] error " << LastErrorMessage();
            m_nProxy.UnStore(GetMaster());
            delete GetMaster();
        }
    }
    else
    {
        m_nWriteBuffer.VistPos() += nLen;
        if (m_nWriteBuffer.Empty())
        {
            m_nWriteBuffer.Reset();
            if (!m_pBrother->m_nReadBuffer.Empty())
            {
                m_nWriteBuffer.Swap(m_pBrother->m_nReadBuffer);
                GetLoop()->RegistSocket(m_pBrother, m_pBrother->GetMarkEvt());
            }
        }
        GetLoop()->RegistSocket(this, GetMarkEvt());
    }
}

void TcpProxyPeerSocket::OnError()
{
    AILOG_INFO() << "TcpProxyPeerSocket::OnErrort " << LastErrorMessage();
    m_nProxy.UnStore(GetMaster()); 
    delete GetMaster(); // from now,the socket and it's brother has been delete.
}

TcpProxy::~TcpProxy()
{
    AILOG_INFO() << "TcpProxy::~TcpProxy()";
    
    for (TcpProxyPeerSocketPool::iterator it = m_nPeerSocketPool.begin(); 
        it != m_nPeerSocketPool.end(); ++it)
    {
        delete *it;
    }
    m_nPeerSocketPool.clear();
    
    delete m_nServerSocket;
}

void TcpProxy::Store(TcpProxyPeerSocket* pSocket)
{
    TcpProxyPeerSocketPool::iterator it = m_nPeerSocketPool.find(pSocket);
    if (it != m_nPeerSocketPool.end())
        m_nPeerSocketPool.insert(pSocket);
}

void TcpProxy::UnStore(TcpProxyPeerSocket* pSocket)
{
    TcpProxyPeerSocketPool::iterator it = m_nPeerSocketPool.find(pSocket);
    if (it != m_nPeerSocketPool.end())
        m_nPeerSocketPool.erase(it);
}

bool TcpProxy::KickOff(CSelectLoop& nLoop)
{
    AILOG_INFO() << "TcpProxy::KickOff " << m_nItem.h1 << ":" << m_nItem.p1
        << "<-->" << m_nItem.h2 << ":" << m_nItem.p2;

    m_nServerSocket = new TcpProxyServerSocket(*this);
    m_nServerSocket->SetAddress(m_nItem.h1, m_nItem.p1);
    
    if (!m_nServerSocket->Open())
    {
        AILOG_ERROR() << "Open socket error:" << CTcpSocket::LastErrorMessage();
        return false;
    }
    
    if (!m_nServerSocket->SetOptNB(true))
    {
        AILOG_ERROR() << "SetOptNB socket error:" << CTcpSocket::LastErrorMessage();
        return false;
    }
    
    if (!m_nServerSocket->Listen())
    {
        AILOG_ERROR() << "Listen socket error:" << CTcpSocket::LastErrorMessage();
        return false;
    }
    
    nLoop.RegistSocket(m_nServerSocket, CSelectLoop::EVT_ACCEPT);
    return true;
}

TcpProxyManager::TcpProxyManager()
{
}

TcpProxyManager::~TcpProxyManager()
{
    AILOG_INFO() << "TcpProxyManager::~TcpProxyManager()";
    for (int i = 0; i < m_nProxyArray.size(); ++i)
    {
        delete m_nProxyArray[i];
    }
    m_nProxyArray.clear();
}

bool TcpProxyManager::KickOff(CSelectLoop& nLoop)
{
    const ProxyItemPool& nPoolCfg = ConfigManager::Instance().GetProxyItemPool();
    for (int i = 0; i < nPoolCfg.size(); ++i)
    {
        const ProxyItem& nItem = nPoolCfg[i];
        m_nProxyArray.push_back(new TcpProxy(nItem));
    }
    
    for (int i = 0; i < m_nProxyArray.size(); ++i)
    {
        if (!m_nProxyArray[i]->KickOff(nLoop))
        {
            return false;
        }
    }
    return true;
}
