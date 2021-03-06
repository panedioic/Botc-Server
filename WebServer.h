#ifndef _WEB_SERVER
#define _WEB_SERVER

#include <string>
#include <string.h>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include "./mongoose/mongoose.h"
#include <./GameHandler.h>

const int MAX_CONNECTION_WEBSOCKET = 1024;
class GameHandler;

class WebServer {
public:
	mg_serve_http_opts s_server_option;
	char* web_dir;
	char* web_port;
	mg_connection* fid_ptr_mappint[MAX_CONNECTION_WEBSOCKET];

	GameHandler* gameHandler;
	pthread_t run_thread;

	WebServer(GameHandler* _gameHandler);
	~WebServer();
	int init(); // 初始化设置
	int start();
	int close();
	int setDir(char* dir);
	int setPort(int port);

	int sendWebSocket(int fid, char* msg);

	//bool Start(); // 启动httpserver
	//bool Close(); // 关闭
	//void AddHandler(const std::string &url, ReqHandler req_handler); // 注册事件处理函数
	//void RemoveHandler(const std::string &url); // 移除时间处理函数
	//static std::string s_web_dir; // 网页根目录 
	//static mg_serve_http_opts s_server_option; // web服务器选项
	//static std::unordered_map<std::string, ReqHandler> s_handler_map; // 回调函数映射表

private:
	static void ev_handler_func(mg_connection* connection, int event_type, void* event_data);
	void ev_handler_func_http(mg_connection* connection, http_message* http_req);
	void ev_handler_func_ws  (mg_connection* connection, int event_type, websocket_message* ws_msg); 
	
	// 静态事件响应函数
	//static void OnHttpWebsocketEvent(mg_connection *connection, int event_type, void *event_data);

	//static void HandleHttpEvent(mg_connection *connection, http_message *http_req);
	//static void SendHttpRsp(mg_connection *connection, std::string rsp);

	//static int isWebsocket(const mg_connection *connection); // 判断是否是websoket类型连接
	//static void HandleWebsocketMessage(mg_connection *connection, int event_type, websocket_message *ws_msg); 
	//static void SendWebsocketMsg(mg_connection *connection, std::string msg); // 发送消息到指定连接
	//static void BroadcastWebsocketMsg(std::string msg); // 给所有连接广播消息
	//static std::unordered_set<mg_connection *> s_websocket_session_set; // 缓存websocket连接

	std::string m_port;    // 端口
	mg_mgr m_mgr;          // 连接管理器
};











#endif