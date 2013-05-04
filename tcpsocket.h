#ifndef __TCP_SOCKET_H__
#define __TCP_SOCKET_H__

#include <string>
#include <vector>
#include <errno.h>

#if defined(WIN32)
typedef int socklen_t;
#define EINTR_HANDLER(x) x
#else
#define EINTR_HANDLER(x) ({                     \
    typeof(x) result;                           \
    do {                                        \
        result = x;                             \
    } while (result == -1 && errno == EINTR);   \
    result;                                     \
})
#endif

class CSelectLoop;
class CTcpSocket
{
private:
    virtual void OnConnect() { }
    virtual void OnAccept() {  }
    virtual void OnRead() {  }
    virtual void OnWrite() {  }
    virtual void OnError() { }
    
public:
    typedef std::vector<CTcpSocket*> PtrArray;

    CTcpSocket() : m_nFd(-1), m_bRegist(false), m_nLoop(NULL) {}
    virtual ~CTcpSocket() { Close(); }
    
    void SetAddress(const std::string& strHost, int nPort)
    { m_strHost = strHost; m_nPort = nPort; }
    
    int GetFd() const { return m_nFd; }
    
    bool Open();
    bool Close();
    bool SetOptNB(bool flag);
    bool Listen(int n = 4);
    bool Accept(CTcpSocket& nNew);
    bool Connect();
    bool Read(void* pBuffer, int nBufferLen, int& nReadDataLen);
    bool Write(const void* pData, int nDataLen, int& nWriteDatalen);

    static int LastError();
    static std::string ErrorMessage(int nCode);
    static std::string LastErrorMessage();
    static bool IsNoneFatal(int nCode);
    static bool IsNoneFatal() { return IsNoneFatal(LastError()); }
    
    void Swap(CTcpSocket& other);
    const std::string AddressToString() const;
    bool GetPeerAddress(std::string& strHost, int& nPort);
    bool GetLocalAddress(std::string& strHost, int& nPort);
    
    CSelectLoop* GetLoop() { return m_nLoop; }
    
    static bool OpenSocketPair(CTcpSocket& nIn, CTcpSocket& nOut);

    const std::string& GetHost() const { return m_strHost; }
    int GetPort() const { return m_nPort; }
    
private:
    //none copyable
    CTcpSocket(const CTcpSocket&);
    CTcpSocket operator=(const CTcpSocket&);
    friend class CSelectLoop;
    
    std::string m_strHost;
    int         m_nPort;
    
private:
    int         m_nFd;
    bool        m_bRegist;
    CSelectLoop* m_nLoop;    
};

#endif

