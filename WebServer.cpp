#include <stdio.h>
#include <string.h>

#include <./WebServer.h>

WebServer::WebServer(GameHandler* _gameHandler) {
	gameHandler = _gameHandler;

    web_dir = new char[256];
    sprintf(web_dir, "./static");
    // const std::string &port
    web_port = new char[8];
    sprintf(web_port, "7001");

    init();
}

WebServer::~WebServer() {
    delete [] web_dir;
    delete [] web_port;
    close();
}

int WebServer::init() {
	s_server_option.enable_directory_listing = "yes";
	s_server_option.document_root = web_dir;
	// s_server_option.extra_headers = "Access-Control-Allow-Origin: *"; // cors

    return 0;
}

int WebServer::start() {
	// Figured out at https://github.com/cesanta/mongoose/issues/821
	struct mg_bind_opts bind_opts;

    memset(&bind_opts, 0, sizeof(bind_opts));
    //bind_opts.user_data = "HELLO DOLLY";
    bind_opts.user_data = (void*)this;

    //mg_bind_opt(&mgr, "9009", ev_handler, bind_opts);
    mg_mgr_init(&m_mgr, NULL);
	//mg_connection* connection = mg_bind(&m_mgr, web_port, ev_handler_func);
	mg_connection* connection = mg_bind_opt(&m_mgr, web_port, ev_handler_func, bind_opts);

    if(!connection){
        return -1;
    }

	// for both http and websocket
	mg_set_protocol_http_websocket(connection);

	printf("[Info ] [Server] Starting http server at port: %s\n", m_port.c_str());
	while (1){
        mg_mgr_poll(&m_mgr, 500);
    }

	return 0;
}

int WebServer::close() {
    mg_mgr_free(&m_mgr);

    return 0;
}

void WebServer::ev_handler_func(mg_connection *connection, int event_type, void *event_data) {
	WebServer* ctx = (WebServer*)connection->mgr->user_data;
    switch (event_type) {
        case MG_EV_HTTP_REQUEST: {
            http_message* http_req = (http_message*)event_data;
            ctx->ev_handler_func_http(connection, http_req);
            break;
        }
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
        case MG_EV_WEBSOCKET_FRAME:
        case MG_EV_CLOSE: {
            websocket_message* ws_message = (struct websocket_message*)event_data;
            ctx->ev_handler_func_ws(connection, event_type, ws_message);
            break;
        }
    }

    return;
}

void WebServer::ev_handler_func_http(mg_connection* connection, http_message* http_req) {
    // 1. It is only designed for couple of file, so there is no need for router.
    // 2. I thought about load those files in memory for optimization, but just like above, it is not necessary for a demo to do this.

    // std::string req_str = std::string(http_req->message.p, http_req->message.len);
	// printf("got request: %s\n", req_str.c_str());
    //
    //std::string url = std::string(http_req->uri.p, http_req->uri.len);
	//std::string body = std::string(http_req->body.p, http_req->body.len);

    if (mg_vcmp(&http_req->uri, "/") == 0){ // index page
        mg_serve_http(connection, http_req, s_server_option);

    } else if (mg_vcmp(&http_req->uri, "/api/hello") == 0) { // test 1
		// 直接回传
		//SendHttpRsp(connection, "welcome to httpserver");

        // 必须先发送header, 暂时还不能用HTTP/2.0
	    mg_printf(connection, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
	    // 以json形式返回
	    mg_printf_http_chunk(connection, "{ \"result\": %s }", "welcome to httpserver");
	    // 发送空白字符快，结束当前响应
	    mg_send_http_chunk(connection, "", 0);

	} else if (mg_vcmp(&http_req->uri, "/api/hello") == 0) { // test 2
		// 简单post请求，加法运算测试
		char n1[100], n2[100];
		double result;

		/* Get form variables */
		mg_get_http_var(&http_req->body, "n1", n1, sizeof(n1));
		mg_get_http_var(&http_req->body, "n2", n2, sizeof(n2));

		/* Compute the result and send it back as a JSON object */
		result = strtod(n1, NULL) + strtod(n2, NULL);
		// SendHttpRsp(connection, std::to_string(result)); */
        char result_str[1024];
        sprintf(result_str, "{ \"result\": %f }", result);
        mg_printf(connection, "HTTP/1.1 200 OK\r\n"
			                  "Content-Type: Application/json\n"
			                  "Cache-Control: no-cache\n"
			                  "Content-Length: %d\n"
			                  "Access-Control-Allow-Origin: *\n\n"
			                  "%s\n", (int)strlen(result_str), result_str);

	} else {
		mg_printf(
			connection,
			"%s",
			"HTTP/1.1 404 Not Found\r\n" 
			"Content-Length: 0\r\n\r\n");
	}
}

void WebServer::ev_handler_func_ws(mg_connection* connection, int event_type, websocket_message* ws_msg) {
	int fid = -1;
	int uid = -1;

    switch (event_type) {
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
            char addr[32];
            mg_sock_addr_to_str(&connection->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
            printf("[Info ] [Server] New wsclient [id] [%s] connected!\n", addr);

			for(fid = 0; fid < MAX_CONNECTION_WEBSOCKET; ++fid) {
				if(fid_ptr_mappint[fid] == NULL){
					break;
				}
			}
			if(fid == MAX_CONNECTION_WEBSOCKET){
				printf("[Error] [Server] Connection max out!\n");
				return;
			}
			fid_ptr_mappint[fid] = connection;

			gameHandler->newConnection(fid, 2);

			char buff[1024] = {0};
			sprintf(buff, "[Info ] [Server] Client websocket connected!\n");
			sendWebSocket(fid, buff);

            break;
        }
		case MG_EV_WEBSOCKET_FRAME: {
			// find fid
			for(fid = 0; fid < MAX_CONNECTION_WEBSOCKET; ++fid){
				if(fid_ptr_mappint[fid] == connection) {
					break;
				}
			}
			if(fid == MAX_CONNECTION_WEBSOCKET){
				return;
			}
			// find uid
			for(uid = 0; uid < MAX_CONNECTION_WEBSOCKET; ++uid){
				if(gameHandler->playerArray[uid]->fid == fid){
					break;
				}
			}
			if(uid == MAX_CONNECTION_WEBSOCKET){
				return;
			}

			mg_str received_msg = {
				(char *)ws_msg->data, ws_msg->size
			};

			char buff[1024] = {0};
			strncpy(buff, received_msg.p, received_msg.len); // must use strncpy, specifiy memory pointer and length
			gameHandler->recvSocket(uid, buff, strlen(buff));

			break;
		}
		case MG_EV_CLOSE: {
			//if (isWebsocket(connection)) {
			if (connection->flags & MG_F_IS_WEBSOCKET) {
				printf("[Info ] [Server] A client websocket closed.\n");
				// find fid
				for(fid = 0; fid < MAX_CONNECTION_WEBSOCKET; ++fid){
					if(fid_ptr_mappint[fid] == connection) {
						break;
					}
				}
				if(fid == MAX_CONNECTION_WEBSOCKET){
					return;
				}
				gameHandler->closeConnection(fid, 2);
			}

			break;
		}
    }

	return;
}

int WebServer::sendWebSocket(int fid, char* msg) {
	if(fid_ptr_mappint[fid] == NULL){
		return -1;
	}
	mg_send_websocket_frame(fid_ptr_mappint[fid], WEBSOCKET_OP_TEXT, msg, strlen(msg));
	return 0;
}