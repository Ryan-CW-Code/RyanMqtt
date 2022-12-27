

#include "platformNetwork.h"

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
    struct addrinfo *addrList = NULL;
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,     // 指定返回地址的地址族
        .ai_socktype = SOCK_STREAM, // 指定返回地址的地址族
        .ai_protocol = IPPROTO_IP}; // 指定socket的协议

    if (getaddrinfo(host, port, &hints, &addrList) != 0)
    {
        result = RyanSocketFailedError;
        goto exit;
    }

    if (NULL == addrList)
        goto exit;

    platformNetwork->socket = socket(addrList->ai_family, addrList->ai_socktype, addrList->ai_protocol);
    if (platformNetwork->socket < 0)
    {
        result = RyanSocketFailedError;
        goto exit;
    }

    if (connect(platformNetwork->socket, addrList->ai_addr, addrList->ai_addrlen) != 0)
    {
        platformNetworkClose(userData, platformNetwork);
        result = RyanMqttSocketConnectFailError;
        goto exit;
    }

exit:
    if (NULL != addrList)
        freeaddrinfo(addrList);
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

        setsockopt(platformNetwork->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)); // 设置接收超时

        recvResult = recv(platformNetwork->socket, recvBuf, recvLen - offset, 0);
        if (recvResult < 0)
        {
            if ((errno == EAGAIN ||      // 套接字已标记为非阻塞，而接收操作被阻塞或者接收超时
                 errno == EWOULDBLOCK || // 发送时套接字发送缓冲区已满，或接收时套接字接收缓冲区为空
                 errno == EINTR))        // 操作被信号中断
                break;

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

        setsockopt(platformNetwork->socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval)); // 设置发送超时

        sendResult = send(platformNetwork->socket, sendBuf, sendLen - offset, 0);
        if (sendResult < 0)
        {
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
