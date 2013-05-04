#ifndef __TCP_RPOXY_H__
#define __TCP_RPOXY_H__

#include "selectloop.h"
#include "config.h"
#include <vector>
#include <set>

class TcpProxy;
class TcpProxyServerSocket : public CTcpSocket
{
public:
    TcpProxyServerSocket(TcpProxy& n) : m_nProxy(n) {}
    virtual ~TcpProxyServerSocket();
    virtual void OnAccept();
    
private:
    TcpProxy& m_nProxy;
};

class TcpProxyPeerSocket : public CTcpSocket
{
public:
    TcpProxyPeerSocket(TcpProxy& n, bool bMaster) : m_nProxy(n), 
        m_pBrother(NULL), m_bMaster(bMaster) {
    }
    virtual ~TcpProxyPeerSocket();
    
    virtual void OnConnect();
    virtual void OnRead();
    virtual void OnWrite();
    virtual void OnError();
    
    TcpProxyPeerSocket* GetMaster() { return m_bMaster ? this : m_pBrother; }
    
private:
    unsigned int GetMarkEvt();
    
    friend class TcpProxyServerSocket;
    TcpProxy& m_nProxy;
    TcpProxyPeerSocket* m_pBrother;
    bool      m_bMaster;
    
    class Buffer
    {
    public:
        static const int kBufferLen = 1500;
        Buffer() : m_nDataLen(0), m_nVistPos(0)
        { m_pBuffer = new std::string(); m_pBuffer->resize(kBufferLen); }

        ~Buffer() { delete m_pBuffer; }
        
        void* GetBuffer() { return &(*m_pBuffer)[0]; }
        void* GetVistBuffer() { return &(*m_pBuffer)[m_nVistPos]; }
        int GetBufferLen() { return kBufferLen; }
        int& DataLen() { return m_nDataLen; }
        int& VistPos() { return m_nVistPos; }
        int LeftLen() { return m_nDataLen - m_nVistPos; }
        
        void Swap(Buffer& nOther)
        {
            std::swap(m_pBuffer, nOther.m_pBuffer);
            std::swap(m_nDataLen, nOther.m_nDataLen);
            std::swap(m_nVistPos, nOther.m_nVistPos);
        }
        
        bool Empty() const { return m_nDataLen == m_nVistPos; }
        void Reset() { m_nDataLen = 0; m_nVistPos = 0; }
        
    private:
        friend class TcpProxyPeerSocket;
        std::string* m_pBuffer;
        int m_nDataLen;
        int m_nVistPos;
    };
    
    void ProcessProto(Buffer& nBuffer);
    
    Buffer m_nReadBuffer;
    Buffer m_nWriteBuffer;
};

typedef std::set<TcpProxyPeerSocket*> TcpProxyPeerSocketPool;

class TcpProxy
{
public:
    TcpProxy(const ProxyItem& nItem) : m_nItem(nItem), m_nServerSocket(NULL){}
    ~TcpProxy();
    
    bool KickOff(CSelectLoop& nLoop);
    
    const ProxyItem& GetProxyItem() const { return m_nItem; }
    
    void Store(TcpProxyPeerSocket* pSocket);
    void UnStore(TcpProxyPeerSocket* pSocket);
    
private:
    //none copyable
    TcpProxy(const TcpProxy&);
    TcpProxy& operator=(const TcpProxy&);
    
    ProxyItem      m_nItem;
    TcpProxyServerSocket*  m_nServerSocket;
    TcpProxyPeerSocketPool m_nPeerSocketPool;
};
typedef std::vector<TcpProxy*> TcpProxyArray;

class TcpProxyManager
{
public:
    TcpProxyManager();
    ~TcpProxyManager();
    bool KickOff(CSelectLoop& nLoop);
    
private:
    TcpProxyArray m_nProxyArray;
};

#endif
