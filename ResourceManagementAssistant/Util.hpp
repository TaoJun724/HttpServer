#ifndef __UTIL_HPP__ 
#define __UTIL_HPP__  

#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <errno.h>
#include <sstream> 
#include <dirent.h>
#include <fcntl.h>

#define LOG(...) do{ fprintf(stdout, __VA_ARGS__); fflush(stdout); }while(0) 
#define WWWROOT "WWW"
#define MAX_HTTPHDR 4096 //所能接收http头部的最大长度
#define MAX_PATH 256
#define MAX_BUFF 4096

//http协议的状态码描述
std::unordered_map<std::string,std::string> error_desc = {
  {"200", "OK"},//请求成功
  {"400", "Bad Quest"},//错误的请求
  {"403", "Forbidden"},//请求被禁止
  {"404", "Not Found"},//请求失败
  {"405", "Method Not Allowed"},//方法不被允许
  {"413", "Request Entity Too Large"}//请求头过长
};

//文件的类型
std::unordered_map<std::string, std::string> file_type = {
  {"txt", "application/octet-stream"},
  {"rtf", "application/rtf"},
  {"gif", "image/gif"},
  {"jpg", "image/jpeg"},
  {"avi", "video/x-msvideo"},
  {"gz",  "application/x-gzip"},
  {"tar", "application/x-tar"},
  {"mpg","video/mpeg"},
  {"au", "audio/basic"},
  {"midi","audio/midi"},
  {"ra", "audio/x-pn-realaudio"},
  {"ram", "audio/x-pn-realaudio"},
  {"midi","audio/x-midi"},
  {"html", "text/html"},
  {"unknow", "application/octet-stream"}
};



class Util{
  public:
    //将数据进行分割
    static int Split(std::string &src,const std::string &seg,std::vector<std::string> &list){
      int num = 0;
      //key: val\r\nkey: val\r\nkey: val 从key开始先找到\,再找到下一个key
      size_t index = 0;
      size_t pos = 0;
      while(index < src.length())
      {
        pos = src.find(seg,index);
        if(pos == std::string::npos)
          break;
        list.push_back(src.substr(index,pos - index));
        num++;
        index = pos + seg.length();
      }

      if(index < src.length())
      {
        list.push_back(src.substr(index,pos - index));
        num++;
      }
      return num;//num表示总共分割的数据
    }

    //将时间戳转换为格林威治时间
    static void TimeToGMT(time_t t,std::string &gmt)
    {
      //strftime 将一个结构体中的时间按一定的格式进行转换，返回字符串的长度
      //gmtime 将时间戳准换成格林威治时间， 返回一个结构体  
      ////fomat 结构体 %a一周中的第几天  %d一个月中的第几天 ,%b月份的名称 %Y年份
      struct tm *mt = gmtime(&t);
      char tmp[128] = {0};
			//GMT格式举例：Tue,19 Feb 2019 03：00：11 GMT
      int len = strftime(tmp,127,"%a, %d %Y %H:%M:%S GMT", mt);
      gmt.assign(tmp,len);
    }

    //获取状态错误码
    static std::string GetErrNumState(const std::string &code)
    {
      auto it = error_desc.find(code);
      if(it == error_desc.end())
      {
        LOG("Unknow error %s\n", strerror(errno));
        return " Unknow Error";
      }
      return it->second;
    }

    //将数字转换为字符串
    static void DigitTostr(int64_t num,std::string &str)
    {
      std::stringstream ss;
      ss << num;
      str = ss.str();
    } 

    //字符串转为数字
    static int64_t StrToDigit(const std::string &str)
    {
      int64_t num;
      std::stringstream ss;
      ss << str;
      ss >> num;
      return num;
    }

    //生成ETag
    static void MakeETag(int64_t size,int64_t ino,int64_t mtime,std::string& etag)
    {
      std::stringstream ss;
      //std::hex按十六进制输出
      ss<<"\""<<std::hex <<ino<<"-" <<std::hex<<size<<"-"<<std::hex<<mtime << "\"";
      etag = ss.str();
    }

    //获取文件类型
    static void GetMime(std::string &file,std::string &mime)
    {
      size_t pos;
      pos = file.find_last_of(".");
      if(pos == std::string::npos)
      {
        mime = file_type["unknow"];
        return; 
      }

      std::string suffix = file.substr(pos + 1);
      auto it = file_type.find(suffix);
      if(it == file_type.end()) {
        mime = file_type["unkown"];
      }else{
        mime = it->second;
      }
    }
};

//解析的请求信息
class RequestInfo{
  public:
    std::string _method;//请求方法
    std::string _version; //协议版本
    std::string _path_info; //资源路径
    std::string _path_phys; //资源实际路径
    std::string _query_string; //查询字符串
    std::unordered_map<std::string, std::string> _hdr_list; //头部信息中的键值映射
    struct stat _st;//存放文件信息
  public:
    std::string error_code;//存放错误状态
  public:
    void SetError(const std::string str)
    {
      error_code = str;
    }
};



class HttpResponse 
{
  //文件请求(完成文件下载、列表功能)接口
  //CGI请求接口
  private:
    int _cli_sock;
    //Etag:"inode-mtime(最后修改时间)-fsize(文件大小)"\r\n 
    std::string _etag;//表明文件是否修改过 假如浏览器已经下载过的，就不再返回了，通过以下的属性来判断304
    std::string _mtime; //文件的最后一次修改时间
    std::string _cont_len; //请求的长度
    std::string _date;//系统的响应时间 
    std::string _fsize;
    std::string _mime;
  public:
    HttpResponse(int sock):_cli_sock(sock){}
		//初始化一些请求的响应
		//文件的大小 文件的节点号  文件的最后一次修改时间
    bool InitResponse(RequestInfo& req_info)
    {
			//Date
      Util::TimeToGMT(req_info._st.st_mtime, _mtime);
   		//ETAG
	 		Util::MakeETag(req_info._st.st_ino, req_info._st.st_mtime, req_info._st.st_size, _etag);
      time_t t = time(NULL);
      Util::TimeToGMT(t, _date);
			//Last-Modified
      Util::DigitTostr(req_info._st.st_size, _fsize);
      Util::GetMime(req_info._path_info, _mime);
      return true;
    }


    bool SendData(const std::string& str)
    {
      if(send(_cli_sock, str.c_str(), str.length(), 0) < 0){
        return false;
      }
      return true;
    }

    bool SendCData(const std::string& data)
    {
      //判断协议是否为1.1
      if(data.empty())
      {
        SendData("0\r\n\r\n");
        return true;
      }

      std::stringstream ss;
      ss << std::hex <<data.length() << "\r\n";
      SendData(ss.str());
      ss.clear();
      SendData(data);
      SendData("\r\n");
      return true;
    }

    //处理错误响应
    bool ErrHandler(RequestInfo& info)
    {
      //首行  协议版本 状态码  状态描述\r\n
      //头部 Cotent-Length Date
      //空行
      //正文 rsp_body = "<html><body><h1><404><h1></body></html>"


      time_t t =time(NULL);
      std::string gmt;
      Util::TimeToGMT(t, gmt);

      std::string rsp_body;
      rsp_body = "<html><body><h1>" + info.error_code;
      rsp_body += "<h1></body></html>";
      std::string cont_len;
      Util::DigitTostr(rsp_body.length(), cont_len);

      std::string rsp_header;
      rsp_header = info._version + " " + info.error_code + " "; //首行  协议版本 状态码 
      rsp_header += Util::GetErrNumState(info.error_code) + "\r\n";// 状态描述\r\n
      rsp_header += "Date: " + gmt + "\r\n";
      rsp_header += "Content-Length: " + cont_len + "\r\n\r\n";
      //先发送头部，再发送正文
      send(_cli_sock, rsp_header.c_str(), rsp_header.length(), 0);
      send(_cli_sock, rsp_body.c_str(), rsp_body.length(), 0);
      return true;
    }

    void CGIHandler(RequestInfo &info){
      InitResponse(info);
      ProcessCGI(info);
    }
    bool ProcessFile(RequestInfo & info) //文件下载功能
    {
      LOG("进入下载\n");
      std::string rsp_header;
      rsp_header = info._version + " 200 OK\r\n";
      rsp_header += "Connection: close\r\n";
      rsp_header += "Content-Type: " + _mime + "\r\n";
      rsp_header += "Content-Length: " + _fsize + "\r\n"; //
      rsp_header += "ETag: " + _etag + "\r\n";
      rsp_header += "Last-Modified: " + _mtime + "\r\n";
      rsp_header += "Date: " + _date + "\r\n\r\n";
      SendData(rsp_header);

      LOG("rsp_header [ %s  ]\n", rsp_header.c_str());

      int fd = open(info._path_phys.c_str(), O_RDONLY);
      if(fd < 0){
        info.SetError("400");
        ErrHandler(info);
        return false;
      }
      LOG("open file \n");  
      int rlen = 0;
      char tmp[MAX_BUFF];
      while((rlen = read(fd, tmp, MAX_BUFF)) > 0){
        //不能用string 或char*来发送数据 ，因为可能会在文件中有0  //可能会造成对端关闭
        if(send(_cli_sock, tmp, rlen, 0) < 0)
        {
          fprintf(stdout,"send error");
        }
      }
      close(fd);
      //使用md5sum对文件进行验证
    }

    //文件列表功能
		//组织头部
		//首行
		//Content-Type:text/html\r\n
		//ETag:\r\n
		//Date:\r\n
		//Connection:close\r\n\r\n
		//Transfer-Encoding: chunked\r\n
		//正文：
		//每一个目录下的文件都要组织一个html标签信息
    bool ProcessList(RequestInfo &info)
    {
      LOG("AT ProcessList \n");
      std::string rsp_header;
      rsp_header = info._version + " 200 OK\r\n"; 
      rsp_header += "Connection: close\r\n";
      rsp_header += "Content-Type: text/html\r\n";
      if(info._version == "HTTP/1.1"){
        rsp_header += "Transfer-Encoding: chunked\r\n";
      }

      rsp_header += "ETag: " + _etag + "\r\n";
      rsp_header += "Last-Modified：" + _mtime + "\r\n";
      rsp_header += "Date: " + _date + "\r\n\r\n";

      SendData(rsp_header);
      LOG("[%s] \n" , rsp_header.c_str());
      //recv收到0 表示本端关闭链接 
      std::string rsp_body;
      rsp_body = "<html><head>";
      rsp_body += "<meta charset='UTF-8'>"; 
      rsp_body += "<title>资源管理小助手</title>";
      rsp_body +="</head><body><h1> 当前文件路径" + info._path_info + "</h1>";
      rsp_body += "<form action='/upload' method='POST' enctype='multipart/form-data'>";
      rsp_body += "<input type='file' name='FileUpload' />";
      rsp_body += "<input type='submit' value='上传' />";
      rsp_body += "</form>";
      rsp_body += "<hr /><ol>";
      //rsp_body += "<input  type >" 
      LOG("[%s] \n" , rsp_body.c_str()); 
      SendCData(rsp_body);

      //获取目录下的每一个文件，组织处html信息，chunke传输
      //scandir 判断目录下有那些文件  返回值为当前文件数 
      struct dirent **p_dirent = NULL;
      int  num = scandir(info._path_phys.c_str(), &p_dirent, 0, alphasort);
      for(int i = 0; i < num ; ++i)
      {
        std::string file_html;
        std::string file = info._path_phys + p_dirent[i]->d_name;  //当前文件的全路径
        LOG("当前请求展示路径： %s\n", file.c_str());//要注意此时生成的请求的路径后面不带/， 需要自行添加 
        struct stat st;
        if(stat(file.c_str(), &st) < 0)
        {
          LOG("stat error \n");
          continue;
        }
        std::string mtime;
        Util::TimeToGMT(st.st_mtime, mtime);
        std::string mime;
        Util::GetMime(file, mime);
        std::string filesize;
        Util::DigitTostr(st.st_size / 1024, filesize); //kb

        file_html += "<li><strong><a href='";
        file_html += info._path_info; //string表示加粗的链接
        file_html += p_dirent[i]->d_name ;
        file_html += "'>"; 
        file_html += p_dirent[i]->d_name ;
        file_html += "</a></strong>";
        file_html += "<br /><small>";
        file_html += "modified: " + mtime + "<br />";
        file_html += mime + "-" + filesize + "kbytes";
        file_html += "<br /><br /></small></li>";
        SendCData(file_html);
      }
      rsp_body = "</ol><hr /></body></html>";
      SendCData(rsp_body);
      SendCData("");
      return true;
    };



		//使用外部程序完成CGI请求处理---文件上传
		//将http头信息和正文全部交给子进程处理
		//使用环境变量传递头信息
		//使用管道传递正文数据
		//使用管道接受CGI程序的处理结果
    bool ProcessCGI(RequestInfo &info) //CGI请求处理,进行程序替换，需要直到程序文件名称
    {
      //创建管道
      int in[2];//从父进程向子进程传递正文数据
      int out[2]; //子进程向父进程传递结果
      if((pipe(in) < 0) || (pipe(out) < 0))
      {
        info.SetError("500");
        ErrHandler(info);
        return false;
      }

      pid_t pid;
      pid = fork();
      if(pid < 0){
        info.SetError("500");
        ErrHandler(info);
        return false;
      }else if(pid == 0){
        //设置请求行数据
        setenv("METHOD", info._method.c_str(), 1);
        setenv("PATH_INFO", info._path_info.c_str(), 1);
        setenv("VERSION", info._version.c_str(), 1);
        setenv("QUERY_STRING", info._query_string.c_str(), 1);

        //传递首部字段
        for(auto it = info._hdr_list.begin(); it != info._hdr_list.end(); it++)
        {
          setenv(it->first.c_str(), it->second.c_str(), 1);
        }

        close(in[1]);
        close(out[0]);

        //程序替换后文件描述符就会消失，子进程将运行自己的代码，所以需要进行重定向到标准输入输出
        dup2(in[0], 0);//子进程从标准输入读取数据
        dup2(out[1], 1);//子进程直接打印处理结果传递给父进程

        execl(info._path_phys.c_str(), info._path_phys.c_str(), NULL);
        exit(0);
      }
      //父进程关闭in[0] 与 out[1]
      close(in[0]);
      close(out[1]);

      //父进程向子进程传递正文数据通过out管道，
      auto it = info._hdr_list.find("Content-Length"); //如果没有正文就不传递
      if(it != info._hdr_list.end()){
        char buf[MAX_BUFF] = {0};
        int64_t cont_len = Util::StrToDigit(it->second);

        //只读了一遍可能没读完
        int tlen = 0;
        while(tlen < cont_len){
          //为了防止后面还有数据，造成粘包， 和所能接收到的数据进行比较有多少接多少， 最多接收到缓冲区满
          int len = MAX_BUFF > (cont_len -tlen ) ?(cont_len - tlen) : MAX_BUFF;
          int rlen = recv(_cli_sock, buf, len, 0);
          if(rlen <= 0)
            return false;

          //向管道写入数据会卡住，有大小限制， 管道满了就会阻塞
          if(write(in[1], buf, rlen) < 0)
            return false;
          tlen += rlen;
        }
      }
      //组织响应头部
      std::string rsp_header;
      rsp_header = info._version + " 200 OK\r\n"; rsp_header += "Connection: close\r\n";
      rsp_header += "Content-Type: text/html\r\n";
      rsp_header += "ETag: " + _etag + "\r\n";
      rsp_header += "Last-Modified: " + _mtime + "\r\n";
      rsp_header += "Date: " + _date + "\r\n\r\n";
      SendData(rsp_header);

      while(1){
        //从子进程读取所返回的结果
        char buf[MAX_BUFF] = {0};
        int rlen = read(out[0], buf, MAX_BUFF);
        if(rlen == 0)
          break;
        LOG("读取到结果 %s\n", buf);
        send(_cli_sock, buf, rlen, 0);
      }
      close(in[1]);
      close(out[0]);
      LOG("结束\n");
      //处理错误
      return true;          
    }
};


//请求类：实现http数据的接受与解析
//并对外提供获取处理结果的接口
class HttpRequest{
  private:
    int _cli_sock;
    std::string _head;//头部信息
    RequestInfo _req_info;//解析出来的结果
  public:
    HttpRequest(int sock):_cli_sock(sock) {}

    //接受http的请求头
    bool RecvHttpHeader(RequestInfo& info)
    {
      //限定头部的数据长度
      char buf[MAX_HTTPHDR];
      while(1)
      {
        //把flags设置为MSG_PEEK，仅仅是把tcp 缓冲区中的数据读取到buf中，没有把已读取的数据从tcp 缓冲区中移除，如果再次调用recv()函数仍然可以读到刚才读到的数据。
        int ret = recv(_cli_sock,buf,MAX_HTTPHDR,MSG_PEEK);//当recv=0表示对端关闭连接了，对于send函数触发SIGPIPE信号就是对端关闭连接了。
        LOG("ret : %d\n", ret);
        if(ret <= 0)
        {
          if(errno = EINTR || errno == EAGAIN)  
          {
            //EINTER 表示被信号打断
            //ENGAIN 非阻塞时，缓冲区中没有错误
            _req_info.SetError("500"); //服务器被中断，出错
            return false;
          }
        }
        LOG("[ %s  ]\n", buf);
        char* ptr = strstr(buf, "\r\n\r\n");
        if((ptr == NULL)&&(ret == MAX_HTTPHDR))
        {
          _req_info.SetError("413"); //头部太长
          return false;
        }
        else if(ret >= MAX_HTTPHDR)
        {
          usleep(1000);
          continue;
        }

        LOG("recv header3\n");
        int hdr_len = ptr - buf; //读取到的位置减去起始位置，就是头部的长度
        LOG("hdr_len %d\n", hdr_len);
        _head.assign(buf, hdr_len);
        recv(_cli_sock, buf, hdr_len + 4, 0);//将\r\n\r\n读走
        LOG("header:[%s\n]",_head.c_str());
        break; 
      }
      return true;
    }

    bool PathIsLegal(RequestInfo &info)
    {
      //判断路径是否合法，stat输出的文件信息,通过他的返回值判断文件是否存在,Linux文件名的最大长度为256,是一个宏
      //stat获取状态信息出错，表示无法访问到文件
      LOG("判断文件的是否正确\n");
      if(stat(info._path_phys.c_str(),&info._st) < 0)
      {
        LOG("SetError 404\n");
        info.SetError("404");
        return false;
      }
      LOG("文件正确\n");

      char tmp[MAX_PATH] = {0};
      //realpath是用来将参数path所指的相对路径转换成绝对路径,缺点： 请求路径不存在，会造成段错误,先判断再转换
      realpath(info._path_phys.c_str(),tmp);

      info._path_phys = tmp;
      LOG("所要查找的路径： %s\n", info._path_phys.c_str());
      if(info._path_phys.find(WWWROOT) == std::string::npos){
        info.SetError("403");
        return false;
      }
      return true;
    }

    //解析首行
    bool ParseFirstLine(std::vector<std::string> hdr_list, RequestInfo& info)
    {
      std::vector<std::string> line_list;
      if(Util::Split(hdr_list[0]," ",line_list) != 3)
      {
        info.error_code = "400";
        return false;
      }

      //打印首行
      for(int i = 0;i < 3;i++)
        LOG("FIRSTLINE : %s\n", line_list[i].c_str());  

      std::string url;
      info._method = line_list[0];
      url = line_list[1];
      info._version = line_list[2];

      if(info._method != "GET" && info._method != "POST" && info._method != "HEAD")
      {
        //请求方法不被允许
        info.error_code = "405";
        return false;
      }

      if(info._version != "HTTP/0.9" && info._version != "HTTP/1.0" && info._version != "HTTP/1.1")
      {
        info.error_code = "400";
        return false;
      }

      //url : /upload?key=val&key=val
      size_t pos = url.find("?");
      if(pos == std::string::npos){
        info._path_info = url;
      }else{
        info._path_info = url.substr(0, pos);
        info._query_string = url.substr(pos + 1);
      }

      info._path_phys = WWWROOT + info._path_info;
      LOG("要请求的路径：%s\n", info._path_info.c_str());

      return PathIsLegal(info);
    }

    //解析http头部
    bool ParseHttpHeader(RequestInfo& info)
    {
      //请求头解析
      //请求方法 url 协议版本\r\n
      //key: val\r\nkeyval
      std::vector<std::string> hdr_list;
      Util::Split(_head,"\r\n",hdr_list);
      //查看解析好的数据
      // for(int i = 0; i < hdr_list.size(); i++)
      // LOG("%s\n", hdr_list[i].c_str());

      if(ParseFirstLine(hdr_list, info) == false)
        return false;

      for(size_t i = 1; i < hdr_list.size();  ++i)
      {
        size_t pos = hdr_list[i].find(": ");
        info._hdr_list[hdr_list[i].substr(0, pos)] = hdr_list[i].substr(pos + 2);
      }
      return true;
    }    

    //判断文件是否是目录
    bool FileIsDir(RequestInfo &info)
    {
      std::string path = info._path_info; 
      if(info._st.st_mode & S_IFDIR){
        if(path[path.length() - 1] != '/')
          info._path_info.push_back('/');
        std::string phys = info._path_phys;
        if(phys[phys.length() - 1] != '/')
          info._path_phys.push_back('/');
        return true;
      }
      return false;
    }


    //文件处理
    bool FileHandler(RequestInfo &info, HttpResponse &rsp)
    {
      rsp.InitResponse(info); //初化文件响应信息
      if(FileIsDir(info)){ //判断文件请求是否是目录
        rsp.ProcessList(info);  //执行展示
      }else{
        rsp.ProcessFile(info); //执行文件下载
      }
      return true; 
    }

    bool RequestIsCGI(RequestInfo &info)
    {
      if(((info._method == "GET") && !info._query_string.empty() )|| info._method == "POST")
        return true;

      return false;
    }
};


#endif 
