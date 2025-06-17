/**
 * 注意！此接口仅提供思路示例，具体请根据实际进行修改
 *
 */

#define rlogLevel (rlogLvlWarning) // 日志打印等级

#include "platformNetwork.h"
#include "RyanMqttLog.h"

#define tcpConnect (RyanMqttBit1)
#define tcpSend (RyanMqttBit2)
#define tcpClose (RyanMqttBit3)
#define tcpRecv (RyanMqttBit4)

static char g_resolveIp[64] = {0};
static osMutexId_t mutex = NULL;
static osSemaphoreId_t sem = NULL;

static void callback_socket_GetIPByHostName(u8 contexId, s32 errCode, u32 ipAddrCnt, u8 *ipAddr)
{
    if (errCode == SOC_SUCCESS_OK)
    {
        memset(g_resolveIp, 0, sizeof(g_resolveIp));
        for (int i = 0; i < ipAddrCnt; i++)
        {
            strcpy((char *)g_resolveIp, (char *)ipAddr);
            rlog_i("socket 获取ip成功: num_entry=%d, resolve_ip:[%s]", i, (u8 *)g_resolveIp);
        }
        osSemaphoreRelease(sem);
    }
    else
    {
        rlog_e("socket 获取ip失败: %d", errCode);
    }
}

static void callback_socket_connect(s32 socketId, s32 errCode, void *customParam)
{
    platformNetwork_t *platformNetwork = (platformNetwork_t *)customParam;
    if (errCode == SOC_SUCCESS_OK && platformNetwork->socket == socketId)
    {
        rlog_i("socket 连接成功: %d", socketId);
        osEventFlagsSet(platformNetwork.mqttNetEventHandle, tcpConnect);
    }
}

static void callback_socket_close(s32 socketId, s32 errCode, void *customParam)
{
    if (errCode == SOC_SUCCESS_OK)
    {
        rlog_w("关闭socket成功: %d", socketId);
    }
    else
    {
        rlog_e("关闭socket失败 socketId=%d,error_cause=%d", socketId, errCode);
    }
}

static void callback_socket_read(s32 socketId, s32 errCode, void *customParam)
{
    platformNetwork_t *platformNetwork = (platformNetwork_t *)customParam;
    if (SOC_SUCCESS_OK == errCode && platformNetwork->socket == socketId)
    {
        rlog_w("socket接收到数据: %d", socketId);
        osEventFlagsSet(platformNetwork.mqttNetEventHandle, tcpRecv);
    }
}

static void callback_socket_accept(s32 listenSocketId, s32 errCode, void *customParam)
{
}

static ST_SOC_Callback callback_soc_func =
    {
        callback_socket_connect,
        callback_socket_close,
        callback_socket_accept,
        callback_socket_read,
};

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
    u8 nw_state = 0;
    int32_t eventId;
    char resolveIp[64] = {0};

    // 如果第一次connect就创建事件标志组，否则情况事件标志组标志位
    if (NULL == mutex)
    {
        const osMutexAttr_t myMutex01_attributes = {
            .attr_bits = osMutexRecursive | osMutexPrioInherit | osMutexRobust};
        mutex = osMutexNew(&myMutex01_attributes);
    }
    if (NULL == sem)
    {
        sem = osSemaphoreNew(1, 0, NULL);
    }

    // 如果第一次connect就创建事件标志组，否则情况事件标志组标志位
    if (NULL == platformNetwork.mqttNetEventHandle)
        platformNetwork.mqttNetEventHandle = osEventFlagsNew(NULL);

    Ql_SOC_Register(callback_soc_func, platformNetwork); // 注册socket回调函数

    // 获取网络连接状态
    Ql_GetCeregState(&nw_state);
    if ((1 != nw_state) && (5 != nw_state))
    {
        result = RyanMqttSocketConnectFailError;
        goto __exit;
    }

    osMutexAcquire(mutex, osWaitForever);
    // 清除接收标志位
    osSemaphoreAcquire(sem, 0);
    // 解析域名
    s32 getHostIpResult = Ql_IpHelper_GetIPByHostName(0,
                                                      (u8 *)host,
                                                      callback_socket_GetIPByHostName);
    if (SOC_SUCCESS_OK != getHostIpResult && SOC_NONBLOCK != getHostIpResult)
    {
        result = RyanMqttSocketConnectFailError;
        osMutexRelease(mutex);
        goto __exit;
    }

    if (osOK != osSemaphoreAcquire(sem, 10 * 1000))
    {
        result = RyanMqttSocketConnectFailError;
        osMutexRelease(mutex);
        goto __exit;
    }
    memcpy(resolveIp, g_resolveIp, sizeof(resolveIp));
    osMutexRelease(mutex);

    // 创建socket
    platformNetwork->socket = Ql_SOC_Create(0, SOC_TYPE_TCP);
    if (platformNetwork->socket < 0)
    {
        result = RyanSocketFailedError;
        goto __exit;
    }

    // 等待连接成功
    s32 connectResult = Ql_SOC_Connect(platformNetwork->socket, (u8 *)resolveIp, port);
    if (SOC_SUCCESS_OK != connectResult && SOC_NONBLOCK != connectResult)
    {
        platformNetworkClose(userData, platformNetwork);
        result = RyanMqttSocketConnectFailError;
        goto __exit;
    }

    eventId = osEventFlagsWait(mqttNetEventHandle, tcpConnect, osFlagsWaitAny, 10000);
    if (tcpConnect != eventId)
    {
        platformNetworkClose(userData, platformNetwork);
        result = RyanMqttSocketConnectFailError;
        goto __exit;
    }

__exit:
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
    int32_t eventId;
    RyanMqttTimer_t timer = {0};

    if (-1 == platformNetwork->socket)
        return RyanSocketFailedError;

    RyanMqttTimerCutdown(&timer, timeout);
    while ((offset < recvLen) && (0 != timeOut2))
    {

        recvResult = Ql_SOC_Recv(platformNetwork->socket, (u8 *)(recvBuf + offset), recvLen - offset);
        if (recvResult > 0)
        {
            offset += recvResult;
        }
        else
        {
            eventId = osEventFlagsWait(mqttNetEventHandle, tcpRecv, osFlagsWaitAny, timeOut2);
            if (tcpRecv == eventId)
            {
                recvResult = Ql_SOC_Recv(platformNetwork->socket, (u8 *)(recvBuf + offset), recvLen - offset);
                if (recvResult < 0) // 小于零，表示错误，个别错误不代表socket错误
                {
                    if (recvResult != SOC_NONBLOCK &&
                        recvResult != SOC_ERROR_TIMEOUT)
                    {
                        rlog_e("recv失败 result: %d, recvLen: %d, eventId: %d", recvResult, recvLen, eventId);
                        return RyanSocketFailedError;
                    }

                    break;
                }

                offset += recvResult;
            }
        }

        timeOut2 = RyanMqttTimerRemain(&timer);
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
    int32_t eventId;
    RyanMqttTimer_t timer = {0};

    if (-1 == platformNetwork->socket)
        return RyanSocketFailedError;

    RyanMqttTimerCutdown(&timer, timeout);

    while ((offset < sendLen) && (0 != timeOut2))
    {

        sendResult = Ql_SOC_Send(platformNetwork->socket, (u8 *)(sendBuf + offset), sendLen - offset);
        if (sendResult < 0) // 小于零，表示错误，个别错误不代表socket错误
        {
            if (sendResult != SOC_NONBLOCK &&
                sendResult != SOC_ERROR_TIMEOUT)
                return RyanSocketFailedError;
        }
        offset += sendResult;
        timeOut2 = RyanMqttTimerRemain(&timer);
    }

    // osDelay(1000);

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
        Ql_SOC_Close(platformNetwork->socket);
        platformNetwork->socket = -1;

        // todo 这里还是推荐在close的时候把时间标志组删除，否则没有别的地方可以调用删除函数。
        if (platformNetwork.mqttNetEventHandle)
        {
            osEventFlagsDelete(platformNetwork.mqttNetEventHandle);
            platformNetwork.mqttNetEventHandle = NULL;
        }
    }

    return RyanMqttSuccessError;
}
