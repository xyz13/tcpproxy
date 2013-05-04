#ifndef __SELECT_LOOP_H__
#define __SELECT_LOOP_H__

#include "tcpsocket.h"
#include <map>

/*
class CTcpSocketDemo : public CTcpSocket
{
public:
    virtual bool OnConnect()
    {
        GetLoop().RegistSocket(this, CSelectLoop::EVT_READ);
    }
    
    virtual bool OnRead()
    {
        ...do read
        GetLoop().RegistSocket(this, CSelectLoop::EVT_WRITE);
    }
    ...
};
void Funx()
{
    //create new CTcpSocket
    CTcpSocketDemo nCTcpSocket;
    nCTcpSocket.SetOptNB(true);
    nCTcpSocket.SetAddress("192.168.1.1", 80);
    
    ....
    
    CSelectLoop nRunLoop;
    if (!nCTcpSocket.Connect())
    {
        if (CTcpSocket::IsNoneFatal(CTcpSocket::LastErrorCode()))
            nRunLoop.RegistSocket(&nCTcpSocket, CSelectLoop::EVT_CONNECT);
        else
        {
            ... error process
            exit(-1);
        }
    }
    
    nRunLoop.RunLoop();
    
    ....
}
*/
class CSelectLoop
{
public:
    enum {
        EVT_CONNECT = 0x01,
        EVT_ACCEPT = 0x02,
        EVT_READ = 0x04,
        EVT_WRITE = 0x08
    };
    CSelectLoop();
    ~CSelectLoop();  
    bool RunLoop();
    void Waitup();
    void Quit();
    void QuitUntil();
    
    //每次进来必须重新注册
    void RegistSocket(CTcpSocket* pSocket, unsigned int nEvt);
    //取消注册
    void UnRegistSocket(CTcpSocket* pSocket);
    unsigned int GetRegistEvts(CTcpSocket* pSocket);

private:
    //none copyable
    CSelectLoop(const CSelectLoop&);
    CSelectLoop& operator=(const CSelectLoop&);
    
    class CWaitupServerSocket : public CTcpSocket
    {
    public:
        virtual void OnRead();
    };
    
    bool CreateWaitup();
    void Cleanup();
    
    CWaitupServerSocket* m_pWaitupServer;
    CTcpSocket* m_pWaitupClient;
    
    struct SocketData
    {
        CTcpSocket* m_pSocket;
        unsigned int m_nEvt;
        SocketData(CTcpSocket* s, unsigned int e) : m_pSocket(s), m_nEvt(e) {}
    };
    typedef std::map<int, SocketData> Fd2SocketDataMap;
    Fd2SocketDataMap m_nWorkingMap;
    bool m_bKeepRunning;
    bool m_bHasQuit;
};

#endif
