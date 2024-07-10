#define rlogEnable 1             // 是否使能日志
#define rlogColorEnable 1        // 是否使能日志颜色
#define rlogLevel (rlogLvlError) // 日志打印等级
#define rlogTag "RyanMqttNet"    // 日志tag

#include "platformNetwork.h"
#include "RyanMqttLog.h"

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
RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host, const char *port)
{
    RyanMqttError_e result = RyanMqttSuccessError;

    // ?线程安全版本，有些设备没有实现，默认不启用。如果涉及多个客户端解析域名请使用线程安全版本
    char buf[512];
    int ret;
    struct hostent hostinfo, *phost;

    if (0 != gethostbyname_r(host, &hostinfo, buf, sizeof(buf), &phost, &ret))
    {
        result = RyanSocketFailedError;
        goto exit;
    }

    platformNetwork->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (platformNetwork->socket < 0)
    {
        result = RyanSocketFailedError;
        goto exit;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port)); // 指定端口号，这里使用HTTP默认端口80
    server_addr.sin_addr = *((struct in_addr *)hostinfo.h_addr_list[0]);

    // 绑定套接字到主机地址和端口号
    if (connect(platformNetwork->socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        platformNetworkClose(userData, platformNetwork);
        result = RyanMqttSocketConnectFailError;
        goto exit;
    }

    // 非线程安全版本,请根据实际情况选择使用
    // struct hostent *hostinfo;
    // hostinfo = gethostbyname(host);
    // if (NULL == hostinfo)
    // {
    //     result = RyanSocketFailedError;
    //     goto exit;
    // }

    // platformNetwork->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    // if (platformNetwork->socket < 0)
    // {
    //     result = RyanSocketFailedError;
    //     goto exit;
    // }

    // struct sockaddr_in server_addr;
    // memset(&server_addr, 0, sizeof(server_addr));
    // server_addr.sin_family = AF_INET;
    // server_addr.sin_port = htons(atoi(port)); // 指定端口号，这里使用HTTP默认端口80
    // server_addr.sin_addr = *((struct in_addr *)hostinfo->h_addr_list[0]);

    // // 绑定套接字到主机地址和端口号
    // if (connect(platformNetwork->socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    // {
    //     platformNetworkClose(userData, platformNetwork);
    //     result = RyanMqttSocketConnectFailError;
    //     goto exit;
    // }

exit:
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
        return RyanSocketFailedError;

    platformTimerCutdown(&timer, timeout);

    while ((offset < recvLen) && (0 != timeOut2))
    {

        tv.tv_sec = timeOut2 / 1000;
        tv.tv_usec = timeOut2 % 1000 * 1000;

        if (tv.tv_sec <= 0 && tv.tv_usec <= 100)
        {
            tv.tv_sec = 0;
            tv.tv_usec = 100;
        }

        fd_set readset;
        int i, maxfdp1;

        /* 清空可读事件描述符列表 */
        FD_ZERO(&readset);

        /* 将需要监听可读事件的描述符加入列表 */
        FD_SET(platformNetwork->socket, &readset);

        /* 等待设定的网络描述符有事件发生 */
        i = select(platformNetwork->socket + 1, &readset, NULL, NULL, &tv);
        if (i < 0)
        {
            // 下列3种表示没问题,但需要退出接收
            if ((errno == EAGAIN ||      // 套接字已标记为非阻塞，而接收操作被阻塞或者接收超时
                 errno == EWOULDBLOCK || // 发送时套接字发送缓冲区已满，或接收时套接字接收缓冲区为空
                 errno == EINTR))        // 操作被信号中断
                break;

            return RyanSocketFailedError;
        }

        /* 查看 sock 描述符上有没有发生可读事件 */
        if (i > 0 && FD_ISSET(platformNetwork->socket, &readset))
        {
            recvResult = recv(platformNetwork->socket, recvBuf + offset, recvLen - offset, 0);

            if (recvResult <= 0) // 小于零，表示错误，个别错误不代表socket错误
            {
                // 下列3种表示没问题,但需要退出接收
                if ((errno == EAGAIN ||      // 套接字已标记为非阻塞，而接收操作被阻塞或者接收超时
                     errno == EWOULDBLOCK || // 发送时套接字发送缓冲区已满，或接收时套接字接收缓冲区为空
                     errno == EINTR))        // 操作被信号中断
                    break;

                return RyanSocketFailedError;
            }

            offset += recvResult;
        }

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
        return RyanSocketFailedError;

    platformTimerCutdown(&timer, timeout);

    while ((offset < sendLen) && (0 != timeOut2))
    {

        tv.tv_sec = timeOut2 / 1000;
        tv.tv_usec = timeOut2 % 1000 * 1000;

        if (tv.tv_sec <= 0 && tv.tv_usec <= 100)
        {
            tv.tv_sec = 0;
            tv.tv_usec = 100;
        }

        setsockopt(platformNetwork->socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval)); // 设置错做模式为非阻塞

        sendResult = send(platformNetwork->socket, sendBuf + offset, sendLen - offset, 0);

        if (sendResult <= 0) // 小于零，表示错误，个别错误不代表socket错误
        {
            // 下列3种表示没问题,但需要推出发送
            if ((errno == EAGAIN ||      // 套接字已标记为非阻塞，而接收操作被阻塞或者接收超时
                 errno == EWOULDBLOCK || // 发送时套接字发送缓冲区已满，或接收时套接字接收缓冲区为空
                 errno == EINTR))        // 操作被信号中断
                break;

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
        platformNetwork->socket = -1;
    }

    return RyanMqttSuccessError;
}
