#define rlogEnable               // 是否使能日志
#define rlogColorEnable          // 是否使能日志颜色
#define rlogLevel (rlogLvlDebug) // 日志打印等级
#define rlogTag "RyanMqttNet"    // 日志tag

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
 * @brief 连接mqtt服务器
 *
 * @param userData
 * @param platformNetwork
 * @param host
 * @param port
 * @return RyanMqttError_e
 * 成功返回RyanMqttSuccessError， 失败返回错误信息
 */
RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host, uint16_t port)
{
    RyanMqttError_e result = RyanMqttSuccessError;

    struct hostent hostinfo = {0};

    // 解析域名信息
    {
        char buf[512];
        int h_errnop;
        struct hostent *phost;

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
    }

    platformNetwork->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (platformNetwork->socket < 0)
    {
        result = RyanSocketFailedError;
        goto __exit;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port), // 指定端口号
        .sin_addr = *((struct in_addr *)hostinfo.h_addr_list[0]),
    };

    // 绑定套接字到主机地址和端口号
    if (connect(platformNetwork->socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        platformNetworkClose(userData, platformNetwork);
        result = RyanMqttSocketConnectFailError;
        goto __exit;
    }

__exit:
    if (RyanMqttSuccessError != result)
        rlog_e("socket连接失败: %d", result);
    return result;
}

/**
 * @brief 非阻塞接收数据
 *
 * @param userData
 * @param platformNetwork
 * @param recvBuf
 * @param recvLen
 * @param timeout
 * @return RyanMqttError_e
 * socket错误返回 RyanSocketFailedError
 * 接收超时或者接收数据长度不等于期待数据接受长度 RyanMqttRecvPacketTimeOutError
 * 接收成功 RyanMqttSuccessError
 */
RyanMqttError_e platformNetworkRecvAsync(void *userData, platformNetwork_t *platformNetwork, char *recvBuf, int recvLen, int timeout)
{

    int32_t recvResult = 0;
    int32_t offset = 0;
    int32_t timeOut2 = timeout;
    struct timeval tv = {0};
    platformTimer_t timer = {0};

    if (-1 == platformNetwork->socket)
    {
        rlog_e("对端关闭socket连接");
        return RyanMqttNoRescourceError;
    }

    platformTimerCutdown(&timer, timeout);

    while ((offset < recvLen) && (0 != timeOut2))
    {
        tv.tv_sec = timeOut2 / 1000;
        tv.tv_usec = timeOut2 % 1000 * 1000;

        setsockopt(platformNetwork->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)); // 设置错做模式为非阻塞

        recvResult = recv(platformNetwork->socket, recvBuf + offset, recvLen - offset, 0);
        if (0 == recvResult)
        {
            rlog_e("对端关闭socket连接");
            return RyanMqttNoRescourceError;
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
                break;
            }

            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            rlog_e("recvResult: %d, errno: %d  str: %s", recvResult, rt_errno, strerror(rt_errno));
            return RyanSocketFailedError;
        }

        offset += recvResult;
        timeOut2 = platformTimerRemain(&timer);
    }

    if (offset != recvLen)
        return RyanMqttRecvPacketTimeOutError;

    return RyanMqttSuccessError;
}

/**
 * @brief 非阻塞发送数据
 *
 * @param userData
 * @param platformNetwork
 * @param sendBuf
 * @param sendLen
 * @param timeout
 * @return RyanMqttError_e
 * socket错误返回 RyanSocketFailedError
 * 接收超时或者接收数据长度不等于期待数据接受长度 RyanMqttRecvPacketTimeOutError
 * 接收成功 RyanMqttSuccessError
 */
RyanMqttError_e platformNetworkSendAsync(void *userData, platformNetwork_t *platformNetwork, char *sendBuf, int sendLen, int timeout)
{

    int32_t sendResult = 0;
    int32_t offset = 0;
    int32_t timeOut2 = timeout;
    struct timeval tv = {0};
    platformTimer_t timer = {0};

    if (-1 == platformNetwork->socket)
    {
        rlog_e("对端关闭socket连接");
        return RyanMqttNoRescourceError;
    }

    platformTimerCutdown(&timer, timeout);

    while ((offset < sendLen) && (0 != timeOut2))
    {
        tv.tv_sec = timeOut2 / 1000;
        tv.tv_usec = timeOut2 % 1000 * 1000;

        setsockopt(platformNetwork->socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval)); // 设置错做模式为非阻塞

        sendResult = send(platformNetwork->socket, sendBuf + offset, sendLen - offset, 0);
        if (0 == sendResult)
        {
            rlog_e("对端关闭socket连接");
            return RyanMqttNoRescourceError;
        }
        else if (sendResult < 0) // 小于零，表示错误，个别错误不代表socket错误
        {
            int32_t rt_errno = errno; // 似乎5.0.0以上版本需要使用 rt_get_errno

            // 下列表示没问题,但需要退出发送
            if (EAGAIN == rt_errno ||      // 套接字已标记为非阻塞，而接收操作被阻塞或者接收超时
                EWOULDBLOCK == rt_errno || // 发送时套接字发送缓冲区已满，或接收时套接字接收缓冲区为空
                EINTR == rt_errno ||       // 操作被信号中断
                ETIME == rt_errno)         // 计时器过期
            {
                break;
            }

            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            rlog_e("sendResult: %d, errno: %d str: %s", sendResult, rt_errno, strerror(rt_errno));
            return RyanSocketFailedError;
        }

        offset += sendResult;
        timeOut2 = platformTimerRemain(&timer);
    }

    if (offset != sendLen)
        return RyanMqttSendPacketTimeOutError;

    return RyanMqttSuccessError;
}

/**
 * @brief 断开mqtt服务器连接
 *
 * @param userData
 * @param platformNetwork
 * @return RyanMqttError_e
 */
RyanMqttError_e platformNetworkClose(void *userData, platformNetwork_t *platformNetwork)
{

    if (platformNetwork->socket >= 0)
    {
        closesocket(platformNetwork->socket);
        rlog_w("platformNetworkClose socket close %d", platformNetwork->socket);
        platformNetwork->socket = -1;
    }

    return RyanMqttSuccessError;
}
