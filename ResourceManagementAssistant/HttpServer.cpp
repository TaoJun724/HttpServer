#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include "ThreadPool.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "Util.hpp"
#include <signal.h>

const int MAX_LISTEN = 10;
const int MAX_THREAD = 20;

class HttpServer
{
    //为建立的一个tcp服务端程序，接收新连接
    //为新连接组织一个线程池任务，添加到线程池中
    private:
        int _serv_sock;
        ThreadPool *_tp;
    private:
        //http任务处理函数
        static bool HttpHandler(int sock){
            HttpRequest req(sock);//请求处理对象
            HttpResponse rsp(sock);//接受http请求数据并对此作出解析
            RequestInfo info;

            LOG("RecvHttpHeader\n");
            if(req.RecvHttpHeader(info) == false){
              LOG("request err\n");
              goto out;
            }//接收请求出错，需要保存错误信息

            LOG("PraseHttpHeader\n");
            if(req.ParseHttpHeader(info) == false){
              goto out;
            }

          //判断请求否是cgi请求，
            if(req.RequestIsCGI(info)){
            rsp.CGIHandler(info); // 如是cgi 请求，执行Cgi响
            }
         else{
            //不是，执行目录列表/文件下载响应
            req.FileHandler(info, rsp);
          }
    
          close(sock);
            //dd if=/dev/zero of=./test.txt bs=1G
          LOG("请求处理完毕\n");
          return true; 

          out: 
            
           // RequestInfo info1(info);// 测试errhandel
           // info1._err_code = "404";
          rsp.ErrHandler(info);
          close(sock);
          return true;
        }
    public:
       HttpServer()
          :_serv_sock(-1)
           , _tp(NULL) 
        {}

       bool HttpServerInit(std::string ip, uint16_t port) //完成TCP服务端初始化，线程池的初始化
       {
	   _serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if(_serv_sock < 0)
	    {
		LOG("sock error : %s\n", strerror(errno));
		return false;
	    }

           std::cout << "socket creat success!!" <<std::endl;

           //设置套接字选项，使端口地址可以复用
           int ov = 1;
           setsockopt(_serv_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&ov, sizeof(ov));

	   struct sockaddr_in  addr;
	   addr.sin_family = AF_INET;
	   addr.sin_port = htons(port);
	   addr.sin_addr.s_addr = inet_addr(ip.c_str());
          
	   if(bind(_serv_sock, (sockaddr*)&addr, sizeof(addr)) < 0)
	   {   
		LOG("bind error %s\n",strerror(errno));
		close(_serv_sock);
		return false;
	    }

           std::cout << "bind success!!" <<std::endl;
	   if(listen(_serv_sock, MAX_LISTEN) < 0)
	   {
		LOG("liste error %s\n", strerror(errno));
	        close(_serv_sock);
		return false;
	   }

	   _tp = new ThreadPool(MAX_THREAD);
	   if(_tp->ThreadPoolInit() == false){
		LOG("thread pool init error \n");
           }

          std::cout << "ThreadPool Init success!!" <<std::endl;
	   return true;
      }
					

     //开始获取服务器的新连接--创建任务，任务入队
      bool Start()
      {
          while(1){          
            sockaddr_in cli_addr;
            socklen_t len = sizeof(cli_addr);
            int sock = accept(_serv_sock, (sockaddr*)&cli_addr, &len);
            if(sock < 0 ){
              LOG("accept error %s\n", strerror(errno));
              continue;
            }

            std::cout << "accept success!!  sock "<< sock <<std::endl;
            HttpTask ht;
            ht.SetHttpTask(sock, HttpHandler);

            std::cout << "SetHttpTask success!!" <<std::endl;
            _tp->PushTask(ht);

            std::cout << "PushTask  success!!" <<std::endl;
          }
        }
};

void UserTip(char* str){
  std::cout << "please input " << str << " ip port !!" << std::endl;
}

int main(int argc, char* argv[])
{
  if(argc <= 2){
  UserTip(argv[0]);
  return 0;
  }
  std::string ip = argv[1];
  uint16_t port = atoi(argv[2]);
  
  HttpServer server;

  signal(SIGPIPE, SIG_IGN);
  if(server.HttpServerInit(ip, port) == false)
    return -1;

  std::cout << "server start!!" <<std::endl;
  if(server.Start() == false)
    return -1;
}
