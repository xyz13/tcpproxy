
#include "tcpsocket.h"
#include "logger.h"
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <cstdio>
#include "selectloop.h"

#if defined(WIN32)
#include <windows.h>
#include <winsock.h>
class CSocketInit
{
public:
    CSocketInit()
    {
        static bool bInit = false;
        if (!bInit)
        {
            bInit = true;
            WSADATA WSAData;
            if(WSAStartup(MAKEWORD(1, 1), &WSAData ))
            {
                std::cerr << "initializationing error!" << std::endl;
                WSACleanup( );
                exit( 0 );
            }
        }
    }
};
static CSocketInit gCSocketInit;
#else
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#endif

bool CTcpSocket::Open()
{
    if (m_nFd != -1)
    {
        AILOG_ERROR() << "socket has been opened.";
        return false;
    }
    
    m_nFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_nFd <= 0)
    {
        AILOG_ERROR() << "create socket error:" << LastErrorMessage();
        return false;
    }
    
    int nReuseAddr = 1;
    if (setsockopt(m_nFd, SOL_SOCKET, SO_REUSEADDR,
        (char *)&nReuseAddr, sizeof(nReuseAddr)) != 0)
    {
        AILOG_ERROR() << "setsockopt error:" << LastErrorMessage();
        return false;
    }
    
    return true;
}

bool CTcpSocket::Close()
{
    if (m_nFd == -1)
        return true;
    
    if (m_bRegist)
        m_nLoop->UnRegistSocket(this);

#if defined(WIN32)
    return closesocket(m_nFd) == 0;
#else
    return close(m_nFd) == 0;
#endif
}

bool CTcpSocket::SetOptNB(bool flag)
{
#if defined(WIN32)
    unsigned long ul = flag? 1 : 0;
    return ioctlsocket(m_nFd, FIONBIO, (unsigned long*)&ul) != SOCKET_ERROR;
#else
    int x = fcntl(m_nFd, F_GETFL, 0);    
    if (flag)
    {
        if (x & O_NONBLOCK) return true;
        x |= O_NONBLOCK;
    }
    else
    {
        if (!(x & O_NONBLOCK)) return true;
        x &= (~O_NONBLOCK);
    }
    return fcntl(m_nFd, F_SETFL, x) != -1;
#endif
}

static bool GetAddress(const std::string& strHost, int nPort, struct sockaddr_in& nAddress)
{
    struct hostent* h1 = gethostbyname(strHost.c_str());
    if (!h1)
    {
        AILOG_ERROR() << "gethostbyname error:" << CTcpSocket::LastErrorMessage() << 
            " Address=" << strHost << ":" << nPort;
        return false;
    }
    
    struct hostent h = *h1;
    memset(&nAddress, 0, sizeof(nAddress));
    nAddress.sin_family = h.h_addrtype;
    nAddress.sin_port = htons(nPort);
    nAddress.sin_addr = *((struct in_addr*)h.h_addr);
    return true;
}

bool CTcpSocket::Listen(int n)
{
    struct sockaddr_in nAddress;
    if (!GetAddress(m_strHost, m_nPort, nAddress))
        return false;
    
    if (bind(m_nFd, (struct sockaddr*)&nAddress, sizeof(nAddress)) != 0)
    {
        AILOG_ERROR() << "Address=" << inet_ntoa(nAddress.sin_addr) << ":" << ntohs(nAddress.sin_port);
        AILOG_ERROR() << AddressToString() << " bind socket error:" << LastErrorMessage();
        return false;
    }
    
    return listen(m_nFd, n) != -1;
}

bool CTcpSocket::Accept(CTcpSocket& nNew)
{
    nNew.Close();
    
    struct sockaddr_in nInAddress;
    socklen_t nSize = sizeof(nInAddress);
    int fd = accept(m_nFd, (struct sockaddr*)&nInAddress, &nSize);
    if (fd == -1)
        return false;
    
    nNew.m_nFd = fd;
    nNew.m_strHost = inet_ntoa(nInAddress.sin_addr);
    nNew.m_nPort = (unsigned int)ntohs(nInAddress.sin_port);
    return true;
}

bool CTcpSocket::Connect()
{
    struct sockaddr_in nAddress;
    if (!GetAddress(m_strHost, m_nPort, nAddress))
        return false;
        
    const int nRet = connect(m_nFd, (struct sockaddr*)&nAddress, (int)sizeof(nAddress));
    if (nRet == 0)
    {
        return true;
    }
    return false;
}

bool CTcpSocket::Read(void* pBuffer, int nBufferLen, int& nReadDataLen)
{
    nReadDataLen = recv(m_nFd, (char*)pBuffer, nBufferLen, 0);
    return nReadDataLen >= 0;
}

bool CTcpSocket::Write(const void* pData, int nDataLen, int& nWriteDatalen)
{
    nWriteDatalen = send(m_nFd, (char*)pData, nDataLen, 0);
    return nWriteDatalen != -1;
}

int CTcpSocket::LastError()
{
#if defined(WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

std::string CTcpSocket::ErrorMessage(int nCode)
{
#if defined(WIN32)
    CHAR msg[256] = {0};
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        nCode,
        MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), 
        msg,
        256,
        NULL
    );
    return msg;
#else
    return strerror(nCode);
#endif
}

std::string CTcpSocket::LastErrorMessage()
{
    return ErrorMessage(LastError());
}

bool CTcpSocket::IsNoneFatal(int nCode)
{
#if defined(WIN32)
    return nCode == WSAEINPROGRESS || nCode == WSAEWOULDBLOCK;
#else
    return nCode == EINTR || nCode == EWOULDBLOCK || nCode == EAGAIN || nCode == EINPROGRESS;
#endif
}

void CTcpSocket::Swap(CTcpSocket& other)
{
    std::swap(m_strHost, other.m_strHost);
    std::swap(m_nPort, other.m_nPort);
    std::swap(m_nFd, other.m_nFd);
}

const std::string CTcpSocket::AddressToString() const
{
    char buffer[256];
    sprintf(buffer, "%s:%d", m_strHost.c_str(), m_nPort);
    return buffer;
}

bool CTcpSocket::GetPeerAddress(std::string& strHost, int& nPort)
{
    struct sockaddr_in nAddress;
    socklen_t nSize = sizeof(nAddress);
    if (getpeername(m_nFd, (struct sockaddr*)&nAddress, &nSize) != 0)
        return false;

    strHost = inet_ntoa(nAddress.sin_addr);
    nPort = (unsigned int)ntohs(nAddress.sin_port);
    return true;
}

bool CTcpSocket::GetLocalAddress(std::string& strHost, int& nPort)
{
    struct sockaddr_in nAddress;
    socklen_t nSize = sizeof(nAddress);
    if (getsockname(m_nFd, (struct sockaddr*)&nAddress, &nSize) != 0)
        return false;

    strHost = inet_ntoa(nAddress.sin_addr);
    nPort = (unsigned int)ntohs(nAddress.sin_port);
    return true;
}

bool CTcpSocket::OpenSocketPair(CTcpSocket& nIn, CTcpSocket& nOut)
{
#if defined(WIN32)
    CTcpSocket nServer;
    nServer.SetAddress("127.0.0.1", 0);    
    if (!nServer.Open())
    {
        AILOG_ERROR() << "OpenSocketPair[Open] error," << CTcpSocket::LastErrorMessage();
        return false;
    }
    
    if (!nServer.Listen())
    {
        AILOG_ERROR() << "OpenSocketPair[Listen] error," << CTcpSocket::LastErrorMessage();
        return false;
    }
    
    std::string strHost;
    int nPort;
    if (!nServer.GetLocalAddress(strHost, nPort))
    {
        AILOG_ERROR() << "OpenSocketPair[GetLocalAddress] error," << CTcpSocket::LastErrorMessage();
        return false;
    }
    nIn.SetAddress("127.0.0.1", nPort);
    if (!nIn.Open())
    {
        AILOG_ERROR() << "OpenSocketPair[Open client] error," << CTcpSocket::LastErrorMessage();
        return false;
    }
    nIn.SetOptNB(true);
    if (!nIn.Connect())
    {
        if (!CTcpSocket::IsNoneFatal())
        {
            nIn.Close();
            AILOG_ERROR() << "OpenSocketPair[Connect] error," << CTcpSocket::LastErrorMessage();
            return false;
        }
    }

    if (!nServer.Accept(nOut))
    {
        nIn.Close();
        AILOG_ERROR() << "OpenSocketPair[Accept] error," << CTcpSocket::LastErrorMessage();
        return false;
    }
    nIn.SetOptNB(false);
    return true;
#else
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
    {
        AILOG_ERROR() << "OpenSocketPair[socketpair] error," << CTcpSocket::LastErrorMessage();
        return false;
    }
    
    nIn.m_nFd = fds[0];
    nOut.m_nFd = fds[1];
    return true;
#endif
}
