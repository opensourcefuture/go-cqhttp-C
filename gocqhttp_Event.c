#include "gocqhttp_Event.h"
#include "gocqhttp_err.h"
#include<process.h>
#include<stdio.h>
#include<stdlib.h>
#include<winsock2.h>
#include<windows.h>
#include<WS2tcpip.h>
#include<string.h>
#include<time.h>
#pragma comment(lib, "ws2_32.lib")

/*socket*/

//socket返回的文件描述符
SOCKET serverSocket;
SOCKET client;			//客户端

//socket数据
SOCKADDR_IN server_addr;//服务端
SOCKADDR_IN client_addr;//客户端

//结构大小
int addr_len = sizeof(SOCKADDR_IN);

void (*event_response)(void* data);				//事件类型响应函数

/*创建Event服务端*/
cqhttp_err init_gocqhttpEvent(const char* ip, const int port, void(*response)(void* data))
{
	char func[70] = "init_gocqhttpEvent";

	if (!response)
		return set_cqhttp_err(NULLError, func, 1, "\"response\"是空指针");

	//初始化
	memset(&server_addr, 0, sizeof(SOCKADDR_IN));
	memset(&head, 0, sizeof(head));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, (void*)&server_addr.sin_addr);

	head.head = NULL;

	event_response = response;//设置事件判断函数

	//动态库初始化
	WSADATA wsaData;
	int e = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (e)
		return set_cqhttp_err(WSAStartupError, func, 0, NULL);

	//创建套接字
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return set_cqhttp_err(SocketInitError, func, 0, NULL);

	//绑定端口
	if (bind(serverSocket, (SOCKADDR*)&server_addr, sizeof(server_addr)) < 0)
		return set_cqhttp_err(BindError, func, 0, NULL);

	//监听端口
	if (listen(serverSocket, 1) < 0)
		return set_cqhttp_err(ListenError, func, 0, NULL);

	//开始
	int accept_count;
	for (accept_count = 0; accept_count < ACCEPT_COUNT_MAX; accept_count++)
	{
		printf("尝试接受连接：%d", accept_count + 1);
		if ((client = accept(serverSocket, (SOCKADDR*)&client_addr, (socklen_t*)&addr_len)) > 0)
			break;
	}
	if (accept_count > 20)
		return set_cqhttp_err(AcceptFailed, func, 0, NULL);

	puts("已接受连接！");
	return set_cqhttp_err(None, func, 0, NULL);
}

/*接收Event数据*/
cqhttp_err recv_event(void)
{
	char func[20] = "recv_event";
	char* msg;
	if (!event_response)				//如果未声明消息响应函数
		return set_cqhttp_err(NULLError, func, 1, "event_response");
	while (1)
	{
		msg = (char*)malloc(2048);
		if (!msg)
			return set_cqhttp_err(NULLError, func, 0, NULL);
		memset(msg, 0, 2048);
		while (recv(client, msg, 2048, 0) < 0);
		if (send(client, Event_Response, strlen(Event_Response), 0) < 0)
			return set_cqhttp_err(NetworkIOError, func, 1, "发送http响应头失败");
		_beginthread(event_response, 0, (void*)msg);	//开始检索事件
	}
}

////////////
/*事件解析*/
////////////

//群消息事件
group_message_event_data group_message_event_analysis(char* data)
{
	char func[70] = "group_message_event_analysis";
	char anonymous[200] = { '\0' };
	char sender[300] = { '\0' };
	group_message_event_data ge_data;
	memset(&ge_data, 0, sizeof(ge_data));
	if (sscanf(data, GROUP_MESSAGE_EVENT_FORM,
				anonymous,
				&ge_data.font,
				&ge_data.group_id,
				ge_data.message,
				&ge_data.message_id,
				&ge_data.message_seq,
				ge_data.message_type,
				ge_data.post_type,
				ge_data.raw_message,
				&ge_data.self_id,
				sender,
				ge_data.sub_type,
				&ge_data.time,
				&ge_data.user_id) == -1)
				cqhttp_err_out(set_cqhttp_err(StringError, func, 1, "sscanf_1"));
	if (strcmp(anonymous, "null"))		//如果anonymous字段不是null
	{
		if (sscanf(anonymous, GROUP_MESSAGE_EVENT_FORM_ANOYMOUS,
					ge_data.anonymous.flag,
					&ge_data.anonymous.id,
					ge_data.anonymous.name) == -1)
			cqhttp_err_out(set_cqhttp_err(StringError, func, 1, "sscanf_2"));
	}
	return ge_data;
}

//私聊事件
private_message_event_data private_message_event_analysis(char* data)
{
	char func[70] = "private_message_event_analysis";
	private_message_event_data pe_data;
	memset(&pe_data, 0, sizeof(pe_data));

	if (strstr(data, "\"sub_type\":\"friend\""))
	{
		if (sscanf(data, PRIVATE_MESSAGE_EVENT_FORM_1,
					&pe_data.font,
					pe_data.message,
					&pe_data.message_id,
					pe_data.message_type,
					pe_data.post_type,
					pe_data.raw_message,
					&pe_data.self_id,
					&pe_data.sender.age,
					pe_data.sender.nickname,
					pe_data.sender.sex,
					&pe_data.sender.user_id,
					pe_data.sub_type,
					&pe_data.target_id,
					&pe_data.time,
					&pe_data.user_id) == -1)
			cqhttp_err_out(set_cqhttp_err(StringError, func, 1, "sscanf_1"));
	}
	else if (strstr(data, "\"sub_type\":\"group\""))
	{
		if (sscanf(data, PRIVATE_MESSAGE_EVENT_FORM_2,
					&pe_data.font,
					pe_data.message,
					&pe_data.message_id,
					pe_data.message_type,
					pe_data.post_type,
					pe_data.raw_message,
					&pe_data.self_id,
					&pe_data.sender.age,
					&pe_data.sender.group_id,
					pe_data.sender.nickname,
					pe_data.sender.sex,
					&pe_data.sender.user_id,
					pe_data.sub_type,
					&pe_data.temp_source,
					&pe_data.time,
					&pe_data.user_id) == -1)
			cqhttp_err_out(set_cqhttp_err(StringError, func, 1, "sscanf_2"));
	}
	else
		cqhttp_err_out(set_cqhttp_err(StringError, func, 1, "字符串匹配失败"));

	return pe_data;
}

/*事件消息检索*/
//上报类型检索
event_type event_type_switch(const char* data)
{
	char func[70] = "event_type_switch";
	
	if (strstr(data, "\"post_type\":\"message\""))
		return message_event;
	else if (strstr(data, "\"post_type\":\"notice\""))
		return notice_event;
	else if (strstr(data, "\"post_type\":\"meta_event\""))
		return meta_event;
	else
		return unknow_event;
}

//消息类型判断
message_type message_type_switch(const char* data)
{
	char func[70] = "message_type_switch";

	if (strstr(data, "\"message_type\":\"private\""))
		return private_message;
	else if (strstr(data, "\"message_type\":\"group\""))
		return group_message;
	else
		return unknow_message;
}