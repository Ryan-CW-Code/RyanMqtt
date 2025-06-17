#define rlogLevel (rlogLvlDebug) // 日志打印等级

#include "platformNetwork.h"
#include "RyanMqttLog.h"

/**
 * @brief 初始化网络接口层
 *
 * @param userData
 * @param platformNetwork
 * @return RyanMqttError_e
 */
RyanMqttError_e platformNetworkInit(void *userData, platformNetwork_t *platformNetwork)
{
    platformNetwork->socket = -1;
    return RyanMqttSuccessError;
}

/**
 * @brief 销毁网络接口层
 *
 * @param userData
 * @param platformNetwork
 * @return RyanMqttError_e
 */
RyanMqttError_e platformNetworkDestroy(void *userData, platformNetwork_t *platformNetwork)
{
    platformNetwork->socket = -1;
    return RyanMqttSuccessError;
}

/**
 * @brief Establishes a TCP connection to an MQTT server at the specified host and port.
 *
 * If the host is an IP address, connects directly without DNS resolution. If the host is a domain name, attempts DNS resolution using a thread-safe method, falling back to a non-thread-safe method if necessary. Allocates and frees temporary memory for DNS resolution as needed. On failure, cleans up resources and returns an appropriate error code.
 *
 * @param host The server hostname or IP address to connect to.
 * @param port The server port number.
 * @return RyanMqttError_e Returns RyanMqttSuccessError on success, or an error code on failure.
 */
RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host, uint16_t port)
{
    RyanMqttError_e result = RyanMqttSuccessError;
    char *buf = NULL;
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port), // 指定端口号
    };

    // 传递的是ip地址，不用进行dns解析，某些情况下调用dns解析反而会错误
    if (INADDR_NONE != inet_addr(host))
    {
        rlog_d("host: %s, 不用dns解析", host);
        server_addr.sin_addr.s_addr = inet_addr(host);
    }
    // 解析域名信息
    else
    {
        rlog_d("host: %s, 需要dns解析", host);
        int h_errnop;
        struct hostent *phost;
        struct hostent hostinfo = {0};

        buf = (char *)platformMemoryMalloc(384);
        if (NULL == buf)
        {
            result = RyanMqttNoRescourceError;
            goto __exit;
        }

        if (0 != gethostbyname_r(host, &hostinfo, buf, sizeof(buf), &phost, &h_errnop))
        {
            rlog_w("平台可能不支持 gethostbyname_r 函数, 再次尝试使用 gethostbyname 获取域名信息");

            // 非线程安全版本,请根据实际情况选择使用
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            struct hostent *phostinfo = gethostbyname(host);
            if (NULL == phostinfo)
            {
                result = RyanMqttNoRescourceError;
                goto __exit;
            }

            hostinfo = *phostinfo;
        }

        server_addr.sin_addr = *((struct in_addr *)hostinfo.h_addr_list[0]);
    }

    platformNetwork->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (platformNetwork->socket < 0)
    {
        result = RyanSocketFailedError;
        goto __exit;
    }

    // 绑定套接字到主机地址和端口号
    if (connect(platformNetwork->socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        platformNetworkClose(userData, platformNetwork);
        result = RyanMqttSocketConnectFailError;
        goto __exit;
    }

__exit:
    if (NULL != buf)
        platformMemoryFree(buf);

    if (RyanMqttSuccessError != result)
        rlog_e("socket连接失败: %d", result);
    return result;
}

/**
 * @brief Performs a non-blocking receive operation on the network socket with a specified timeout.
 *
 * Attempts to read up to recvLen bytes into recvBuf from the socket associated with platformNetwork. The function sets a receive timeout and returns immediately if no data is available or if the operation would block.
 *
 * @param recvBuf Buffer to store received data.
 * @param recvLen Maximum number of bytes to receive.
 * @param timeout Timeout in milliseconds for the receive operation.
 * @return int32_t Number of bytes received on success; 0 if no data is available or operation is interrupted; -1 if the socket is closed or a fatal error occurs.
 */
int32_t platformNetworkRecvAsync(void *userData, platformNetwork_t *platformNetwork, char *recvBuf, size_t recvLen, int32_t timeout)
{
    int32_t recvResult = 0;
    struct timeval tv = {0};

    if (-1 == platformNetwork->socket)
    {
        rlog_e("对端关闭socket连接");
        return -1;
    }

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout % 1000 * 1000;

    setsockopt(platformNetwork->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)); // 设置错做模式为非阻塞

    recvResult = recv(platformNetwork->socket, recvBuf, recvLen, 0);
    if (0 == recvResult)
    {
        rlog_e("对端关闭socket连接");
        return -1;
    }
    else if (recvResult < 0) // 小于零，表示错误，个别错误不代表socket错误
    {
        int32_t rt_errno = errno; // 似乎5.0.0以上版本需要使用 rt_get_errno
        // 下列表示没问题,但需要退出接收
        if (EAGAIN == rt_errno ||      // 套接字已标记为非阻塞，而接收操作被阻塞或者接收超时
            EWOULDBLOCK == rt_errno || // 发送时套接字发送缓冲区已满，或接收时套接字接收缓冲区为空
            EINTR == rt_errno ||       // 操作被信号中断
            ETIME == rt_errno)         // 计时器过期
        {
            return 0;
        }

        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        rlog_e("recvResult: %d, errno: %d  str: %s", recvResult, rt_errno, strerror(rt_errno));
        return -1;
    }

    return recvResult;
}

/**
 * @brief Sends data asynchronously over a socket with a specified timeout.
 *
 * Attempts to send data from the buffer over the network socket in non-blocking mode, using a send timeout. Returns the number of bytes sent on success, 0 if the operation would block or times out, and -1 if the socket is closed or a fatal error occurs.
 *
 * @param sendBuf Pointer to the data buffer to send.
 * @param sendLen Number of bytes to send from the buffer.
 * @param timeout Timeout for the send operation in milliseconds.
 * @return int32_t Number of bytes sent on success, 0 if no data sent due to timeout or non-fatal error, -1 on socket closure or fatal error.
 */
int32_t platformNetworkSendAsync(void *userData, platformNetwork_t *platformNetwork, char *sendBuf, size_t sendLen, int32_t timeout)
{

    int32_t sendResult = 0;
    struct timeval tv = {0};

    if (-1 == platformNetwork->socket)
    {
        rlog_e("对端关闭socket连接");
        return -1;
    }

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout % 1000 * 1000;

    setsockopt(platformNetwork->socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval)); // 设置错做模式为非阻塞

    sendResult = send(platformNetwork->socket, sendBuf, sendLen, 0);
    if (0 == sendResult)
    {
        rlog_e("对端关闭socket连接");
        return -1;
    }
    else if (sendResult < 0) // 小于零，表示错误，个别错误不代表socket错误
    {
        int32_t rt_errno = errno;      // 似乎5.0.0以上版本需要使用 rt_get_errno
                                       // 下列表示没问题,但需要退出发送
        if (EAGAIN == rt_errno ||      // 套接字已标记为非阻塞，而接收操作被阻塞或者接收超时
            EWOULDBLOCK == rt_errno || // 发送时套接字发送缓冲区已满，或接收时套接字接收缓冲区为空
            EINTR == rt_errno ||       // 操作被信号中断
            ETIME == rt_errno)         // 计时器过期
        {
            return 0;
        }

        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        rlog_e("sendResult: %d, errno: %d str: %s", sendResult, rt_errno, strerror(rt_errno));
        return -1;
    }

    return sendResult;
}

/**
 * @brief Closes the network socket and resets the socket descriptor.
 *
 * If the socket is open, closes it and sets the descriptor to -1.
 *
 * @return RyanMqttSuccessError on successful closure.
 */
RyanMqttError_e platformNetworkClose(void *userData, platformNetwork_t *platformNetwork)
{

    if (platformNetwork->socket >= 0)
    {
        close(platformNetwork->socket);
        rlog_w("platformNetworkClose socket close %d", platformNetwork->socket);
        platformNetwork->socket = -1;
    }

    return RyanMqttSuccessError;
}
