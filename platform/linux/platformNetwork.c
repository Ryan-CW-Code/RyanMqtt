#define RyanMqttLogLevel (RyanMqttLogLevelDebug) // 日志打印等级

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
RyanMqttError_e platformNetworkConnect(void *userData, platformNetwork_t *platformNetwork, const char *host,
				       uint16_t port)
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
		// RyanMqttLog_d("host: %s, 不用dns解析", host);
		server_addr.sin_addr.s_addr = inet_addr(host);
	}
	// 解析域名信息
	else
	{
#define dnsBufferSize (384)
		// RyanMqttLog_d("host: %s, 需要dns解析", host);
		int h_errnop;
		struct hostent *phost;
		struct hostent hostinfo = {0};

		buf = (char *)platformMemoryMalloc(dnsBufferSize);
		if (NULL == buf)
		{
			result = RyanMqttNoRescourceError;
			goto __exit;
		}

		if (0 != gethostbyname_r(host, &hostinfo, buf, dnsBufferSize, &phost, &h_errnop))
		{
			RyanMqttLog_w("平台可能不支持 gethostbyname_r 函数, 再次尝试使用 gethostbyname 获取域名信息");

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
	{
		platformMemoryFree(buf);
	}

	if (RyanMqttSuccessError != result)
	{
		RyanMqttLog_e("socket连接失败: %d", result);
	}
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
int32_t platformNetworkRecvAsync(void *userData, platformNetwork_t *platformNetwork, char *recvBuf, size_t recvLen,
				 int32_t timeout)
{
	ssize_t recvResult = 0;
	struct timeval tv = {
		.tv_sec = timeout / 1000,
		.tv_usec = 1000 * timeout % 1000,
	};

	if (platformNetwork->socket < 0)
	{
		RyanMqttLog_e("对端关闭socket连接");
		return -1;
	}

	setsockopt(platformNetwork->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
		   sizeof(struct timeval)); // 设置操作模式为非阻塞

	recvResult = recv(platformNetwork->socket, recvBuf, recvLen, 0);
	if (0 == recvResult)
	{
		RyanMqttLog_e("对端关闭socket连接");
		return -1;
	}

	if (recvResult < 0) // 小于零，表示错误，个别错误不代表socket错误
	{
		int32_t rt_errno = errno; // 似乎RT 5.0.0以上版本需要使用 rt_get_errno
		// 下列表示没问题,但需要退出接收
		if (EAGAIN == rt_errno ||      // 套接字已标记为非阻塞，而接收操作被阻塞或者接收超时
		    EWOULDBLOCK == rt_errno || // 发送时套接字发送缓冲区已满，或接收时套接字接收缓冲区为空
		    EINTR == rt_errno ||       // 操作被信号中断
		    ETIME == rt_errno)         // 计时器过期
		{
			return 0;
		}

		// NOLINTNEXTLINE(concurrency-mt-unsafe)
		RyanMqttLog_e("recvResult: %d, errno: %d  str: %s", recvResult, rt_errno, strerror(rt_errno));
		return -1;
	}

	return (int32_t)recvResult;
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
int32_t platformNetworkSendAsync(void *userData, platformNetwork_t *platformNetwork, char *sendBuf, size_t sendLen,
				 int32_t timeout)
{
	ssize_t sendResult = 0;
	struct timeval tv = {
		.tv_sec = timeout / 1000,
		.tv_usec = 1000 * timeout % 1000,
	};

	if (platformNetwork->socket < 0)
	{
		RyanMqttLog_e("对端关闭socket连接");
		return -1;
	}

	setsockopt(platformNetwork->socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv,
		   sizeof(struct timeval)); // 设置操作模式为非阻塞

	sendResult = send(platformNetwork->socket, sendBuf, sendLen, 0);
	if (0 == sendResult)
	{
		RyanMqttLog_e("对端关闭socket连接");
		return -1;
	}

	if (sendResult < 0) // 小于零，表示错误，个别错误不代表socket错误
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
		RyanMqttLog_e("sendResult: %d, errno: %d str: %s", sendResult, rt_errno, strerror(rt_errno));
		return -1;
	}

	return (int32_t)sendResult;
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
		RyanMqttLog_w("platformNetworkClose socket close %d", platformNetwork->socket);
		close(platformNetwork->socket);
		platformNetwork->socket = -1;
	}

	return RyanMqttSuccessError;
}
