#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
#define SIZE (1024 * 10)

typedef struct HttpRequest{
  char first_line[SIZE];
  char *method;
  char *url;
  char *url_path;
  char *query_string;
  int content_length;
}HttpRequest;

//这个函数需要考虑不同换行符的问题(浏览器发送的换行符不一定是\n,还可能是 \r , \r\n等)
//处理逻辑：
//1.循环从 socket 中读取字符，一次读一个
//2.对读取到的字符进行判定
//3.如果当前字符是 \r
// a）尝试从缓冲区中读取下一个字符，判定下一个字符是 \n，就把这种情况处理为\n
// b）如果下一个字符是其他字符，就把 \r修改成 \n
//4.如果当前字符是\n,就退出循环，函数结束
//5.如果当前字符是其他字符，就把这个字符放到缓冲区中
int ReadLine(int sock, char buf[], ssize_t max_size){
  //按行从 socket 中读取数据 
  char c = '\0';
  ssize_t i = 0; //作为填充缓冲区的下标
  while(i < max_size){
    ssize_t read_size = recv(sock, &c, 1, 0); 
    if(read_size <= 0){
      //此时认为读取数据失败，即使read_size= 0。由于此时我们预期是至少能读到换行标记的，如果还没读到换行标记就结束了
      //说明很可能收到的报文就是非法的
      printf("read_size <= 0\n");
      return -1;
    }
    if(c == '\r'){
      //最后这个参数的意思是，预览下一个字符而不从缓冲区中删除这个字符
      //即下一次调用recv依然读取到的是这个字符
      recv(sock, &c, 1, MSG_PEEK);
      if(c == '\n'){
        //当前的行分隔符是一个 \r\n
        //接下来就把下一个 \n 字符从缓冲区中删掉即可
        recv(sock, &c, 1, 0);
      }else{
        // b）如果下一个字符是其他字符，就把 \r修改成 \n
        c = '\n';
      }
    }
    buf[i++] = c;
    if(c == '\n'){
      break;
    }
  }
  buf[i] = '\0'; 
  return 0;
}

//这个函数如果使用strtok就会出现线程不安全的问题，所以要使用内核提供的配套的strtok_r
ssize_t Split(char first_line[], const char* split_char, char *output[]){
  int output_index = 0;
  char *tmp = NULL;//此处的 temp 必须是栈上的变量(因为每一个线程都有自己独有的线程栈，不会出现线程不安全的问题)
  char *p = strtok_r(first_line, split_char, &tmp);
  while(p != NULL){
    output[output_index++] = p;
    //后续循环调用的时候，第一个参数要填NULL
    //此时函数就会根据上次切分的结果，继续向下切分
    p = strtok_r(NULL, split_char, &tmp);
  }
  return output_index;
}

int ParseFirstLine(char first_line[], char **method_ptr, char **url_ptr){
  char *tokens[100] = {NULL};
  //Split 切分完毕后，就会破坏掉原有的字符串，把其中的分隔符替换成 \0
  ssize_t n = Split(first_line, " ", tokens);
  if(n != 3){
    printf("first_line Split error! n = %ld\n", n);
    return -1;
  }
  //验证 token[2] 是否包含 HTTP 这样的关键字（字符串匹配）
  *method_ptr = tokens[0];
  *url_ptr = tokens[1];
  return 0;
}

int ParseQueryString(char url[], char **url_path_ptr, char **query_string_ptr){
  *url_path_ptr = url;
  char *p = url;
  for(; *p != '\0'; ++p){
    if(*p == '?'){
      //找到了？，说明此时url 中带有 query_string 
      //先把 ？这个字符替换成 \0
      *p = '\0';
      *query_string_ptr = p + 1;
      return 0;
    }
  }
  //如果 url 中没有找到？，说明url 中不存在 query_string
  *query_string_ptr = NULL;
  return 0;
}

int HandlerHeader(int new_sock, int *content_length_ptr){
  char buf[SIZE] = {0};
 
  while(1){
    if(ReadLine(new_sock, buf, sizeof(buf)) < 0){
      printf("ReadLine faild!\n");
      return -1;
    }
    if(strcmp(buf, "\n") == 0){
      //说明读到了空行，此时 header 部分就结束了
      return 0;
    }
    const char *content_len_ptr = "Content-Length: ";
    if(strncmp(buf, content_len_ptr, strlen(content_len_ptr)) == 0){
      *content_length_ptr = atoi(buf + strlen(content_len_ptr));
      //此处代码不能直接 return，因为本函数有两重含义
      //第一重：找到 content_length 的值
      //第二重：把接收缓冲区中收到的数据都读出来，也就是从缓冲区中删除掉，避免粘包问题
    }
  }//end while(1)
}

//这里有一个坑：
//stat函数的第一个参数只能是绝对路径！
int IsDir(const char* file_path){
  struct stat st; 
  //int ret = stat(file_path, &st);
  int ret = stat(file_path, &st);
  if(ret < 0){
    //printf("file_path = %s\n", file_path);
    perror("stat");
    return 0;
  }
  if(S_ISDIR(st.st_mode)){
    return 1;
  }
  return 0;
}

//通常的url ：http://www.baidu.com/indel.html
//服务器看到的路径，也有很多情况下就是/index.html
//此处暂时只处理第二种情况
void HandlerFilePath(const char* url_path, char* file_path){
  //./wwwroot 这个是随意起的名字，此处对于HTTP服务器的根目录名字是没有明确规定的
  //当前服务器要暴露给客户端的文件必须全部放到这个目录下 ：./wwwroot
  //后期可以使用 gflags 这个库将这个路径改为动态生成的,提高程序的可扩展性
  //TODO
  sprintf(file_path, "/home/zanda/SearchEngines/doc_searcher/http/wwwroot/%s", url_path);
  //对于 url_path 还有几种特殊的情况：
  //1.如果url中没有写路径，默认是 /（http服务器的根目录）
  //2.url中写路径了，但是对应的路径是一个目录
  //如果url_path中对应的是一个目录，就尝试访问该目录下的 index.html文件（即入口文件）
  
  //如果url_path最后一个字符是 /，就说明访问的是一个目录
  if(url_path[strlen(url_path)- 1] == '/'){
    strcat(file_path, "index.html");
  }
  //如果 url_path 最后一个字符不是 /，但是访问的仍然是一个目录
  //此时的核心问题就是要如何识别当前路径是一个目录
  if(IsDir(file_path)){
    strcat(file_path, "/index.html");
  }
  //printf("file_path : %s\n", file_path);
  return;
}

size_t GetFileSize(const char* file_path){
  FILE* fp = fopen(file_path, "r");
  fseek(fp, 0, SEEK_END);
  size_t len = ftell(fp);
  fclose(fp);
  return len;
}

int WriteStaticFile(int new_sock, char* file_path){
  //如果打开失败，则文件有可能不存在 
  printf("file_path = %s\n", file_path); 
  int fd = open(file_path, O_RDONLY);
  if(fd < 0){
    perror("open");
    return 404;
  }
  size_t file_size = GetFileSize(file_path);
  //给 socket 写入的数据其实是一个HTTP响应
  const char *first_line = "HTTP/1.1 200 OK\n";
  //此处需要返回的header重点是两个方面：
  //a）Constent-Type,可以忽略，浏览器能自动识别数据类型
  //b) Content-Length,也可以省略，紧接着就会关闭socket
  char header[SIZE] = {0};
  sprintf(header, "Content-Length: %lu\n", file_size);
  const char *blank_line = "\n";
  send(new_sock, first_line, strlen(first_line), 0);
  send(new_sock, header, strlen(header), 0);
  send(new_sock, blank_line, strlen(blank_line), 0);
  //由于接下来的数据拷贝如果采用 write/read 来进行拷贝
  //会涉及到频繁的访问 IO设备，导致效率下降
  //所以这里使用一个特殊的函数，直接在内核中，一次拷贝就解决问题
  //需要注意的是：这个函数第一个参数必须是一个 socket 
  sendfile(new_sock, fd, NULL, file_size);

  close(fd);
  return 200;
}


int HandlerStaticFile(int new_sock, const HttpRequest* req){
  char file_path[SIZE] = {0};
  HandlerFilePath(req->url_path, file_path);
  int err_code = WriteStaticFile(new_sock, file_path);
  return err_code;
}

void HandlerCGIFather(int new_sock, int child_pid, int father_read, int father_write, const HttpRequest* req){
  //1. 对于 POST 把body中的数据写入到管道中
  char c = '\0';
  if(strcasecmp(req->method, "POST") == 0){
    //从socket中读出数据，写入管道中
    //此处无法使用sendfile， 因为这个函数只能把数据写到socket中。
    //所以这里采用一个字节一个字节的从socket中读出来，再写到管道中
    ssize_t i = 0;
    for(; i < req->content_length; ++i){
      read(new_sock, &c, 1);
      write(father_write, &c, 1);
    }
  }
  //2. 父进程需要构造一个完整的HTTP协议数据，对于HTTP协议要求我们按照指定的格式返回数据
  //对于CGI要求CGI程序返回的结果只是BODY部分，HTTP请求的其他部分需要父进程自己构造
  //Content-Type 和 Content-Length部分省略
  const char* first_line = "HTTP/1.1 200 OK\n";
  const char* blank_line = "\n";
  const char* content_type = "Content-Type:text/html;charset=utf-8\n"; //之前没有发送这个选项导致有的浏览器无法识别
  //TODO
  //后面考虑给响应加上这个选项，使用长连接提高CGI程序的响应速度
  //const char* connection = "keep-alive"; 

  send(new_sock, first_line, strlen(first_line), 0);
  send(new_sock, content_type, strlen(content_type), 0);
  send(new_sock, blank_line, strlen(blank_line), 0);
  //send(new_sock, connection, strlen(connection), 0);

  //3. 从管道中尝试读取数据，写回到socket中，father_read对应的是child_write,对于父进程来说
  //child_write 已经关闭了，对于子进程来说，如果CGI程序处理完进程就推出了，进程退出就会关闭
  //child_write,此时就意味着管道的所有写端都关闭，再尝试读，read返回0
  printf("father will be read & write!\n");
  while(read(father_read, &c, 1) > 0){
    write(new_sock, &c, 1);
  }
  //4. 进行进程等待
  //这里不能使用wait，因为服务器会给每一个客户都创建一个线程，每个线程又很可能创建子进程
  //如果是wait等待，那么任何一个子进程结束都可能导致wait返回，这样子进程就不是由对应的线程
  //来回收了
  waitpid(child_pid, NULL, 0);
}

void HandlerCGIChild(int child_read, int child_write,const HttpRequest* req){
  //1. 创建环境变量，以至于进程替换之后依然可用那些必须的数据
  //REQUEST_METHOD, QUERY_STRING, CONTENT_LENGTH
  char method_env[SIZE] = {0};
  //拼接字符串：REQUEST_METHOD=GET
  sprintf(method_env, "REQUEST_METHOD=%s", req->method);
  putenv(method_env);
  if(strcasecmp(req->method, "GET") == 0){
    //设置QUERY_STRING
    char query_string_env[SIZE] = {0};
  
    sprintf(query_string_env, "QUERY_STRING=%s", req->query_string);
    putenv(query_string_env);
  }else{
    //设置CONTENT_LENGTH
    char content_length_env[SIZE] = {0};
    sprintf(content_length_env, "CONTENT_LENGTH=%d", req->content_length);
    putenv(content_length_env);
  }
  //2. 重定向，将子进程的标准输入和标准输出重定向到管道
  printf("will be exchage!\n");
  dup2(child_read, 0);
  dup2(child_write, 1);
  //3. 进程的程序替换
  char file_path[SIZE] = {0};
  HandlerFilePath(req->url_path, file_path);
  //第一个参数是路径，第二个参数是命令行参数,第三个参数是NULL结尾
  execl(file_path, file_path, NULL);
  //4. 错误处理：如果execl执行失败
  //如果此处不退出，则会出现子进程和父进程监听相同端口号的情况，而我们只希望子进程取调用CGI程序，处理
  //客户端连接这样的事情应该只由父进程来完成
  exit(0); 
}


//CGI 是一种协议， 约定了HTTP服务器如何生成动态页面，HTTP服务器需要创建子进程，子进程进行
//程序替换，
int HandlerCGI(int new_sock, const HttpRequest* req){
  int err_code = 200;
  //1.创建一对管道
  int fd1[2], fd2[2];
  pipe(fd1);
  pipe(fd2);
  int father_read = fd1[0];
  int child_write = fd1[1];
  int father_write = fd2[1];
  int child_read = fd2[0];

  //2.创建子进程
  pid_t ret = fork();
  //3.父进程流程
  if(ret > 0){
    close(child_read);
    close(child_write);
    HandlerCGIFather(new_sock, ret, father_read, father_write, req);
  }else if(ret == 0){
  //4.子进程流程
    close(father_read);
    close(father_write);
    HandlerCGIChild(child_read, child_write, req);
  }else{
    err_code = 404;
  }
  return err_code;
}

//明天把这里更改为调用外部的静态html资源来展示404页面
void Handler404(int new_sock){
  //构造一个错误处理的页面,严格遵守HTTP响应的格式
  const char* first_line = "HTTP/1.1 404 Not Found\n"; 
  const char *blank_line = "\n";
  //body 部分的内容就是HTML
  const char *body ="<head><meta http-equiv=\"content-type\""
                    "content=\"text/html;charset=utf-8\"></head>" 
                    "<h1>你的页面被喵星人吃掉了！！！</h1>";
  char content_length[SIZE] = { 0 };
  sprintf(content_length, "Content-Length: %lu\n", strlen(body));
  send(new_sock, first_line, strlen(first_line), 0);
  send(new_sock, content_length, strlen(content_length), 0);
  send(new_sock, blank_line, strlen(blank_line), 0);
  send(new_sock, body, strlen(body), 0);

}

void HandlerRequest(int64_t new_sock){
  //1.读取请求并解析
  // a)从 socket 中读出HTTP请求的首行
  int err_code = 200;
  HttpRequest req;
  memset(&req, 0, sizeof(req));
  if(ReadLine(new_sock, req.first_line, sizeof(req.first_line) - 1) < 0){
    printf("\nReadLine first_line failed\n");
    //  简略考虑，对于错误的处理情况，统一返回404
    err_code = 404;
    goto END;
  }
  printf("\nfirst_line = %s\n", req.first_line); //OK
  // b)解析首行，获取到方法，url ，版本号（暂不考虑）
  if(ParseFirstLine(req.first_line, &req.method, &req.url) < 0){
    printf("\nParseFirstLine failed! first_line = %s\n", req.first_line);
    err_code = 404;
    goto END;
  }
  // c)对 url 再进行解析，解析出其中的 url_path， query_string
  if(ParseQueryString(req.url, &req.url_path, &req.query_string) < 0){
    printf("\nParseQueryString failed! url = %s\n", req.url);
    err_code = 404;
    goto END;
  }
  // d)读取并解析 header 部分（简略考虑，只保留content_length，其他的header 内容直接丢弃
  //   后面自己做的时候，把有用的部分都保存下来
  if(HandlerHeader(new_sock, &req.content_length) < 0){
    printf("HandlerHeader faild!\n");
    err_code = 404;
    goto END;
  }
  //2.根据请求的详细情况执行静态页面逻辑还是动态页面逻辑
  // a)如果是GET请求，并且没有query_string，就认为是静态页面
  // b)如果是GET请求，并且有query_string，就可以根据query_string参数内容来动态计算生成页面了
  // c)如果是POST请求，就认为是动态页面（简略考虑）
  // d)如果是其他请求，简略考虑不支持其他请求，如果是真实的HTTP服务器，还是要支持其他的请求的
  if(strcasecmp(req.method, "GET") == 0 && req.query_string == NULL){
    //生成静态页面
    err_code = HandlerStaticFile(new_sock, &req);
  }else if(strcasecmp(req.method, "GET") == 0 && req.query_string != NULL){
    printf("url_path = %s\n", req.url_path);
    printf("query_string = %s\n", req.query_string);
    printf("Get-> CGI start\n");
    err_code = HandlerCGI(new_sock, &req);
  }else if(strcasecmp(req.method, "PUT") == 0){
    printf("Put-> CGI start\n");
    err_code = HandlerCGI(new_sock, &req);
  }else{
    //为了简略考虑，其他方法不支持处理
    printf("method not support! method = %s\n", req.method);
    err_code = 404;
    goto END;
  }
END:
  //这里处理收尾工作
  if(err_code != 200){
    Handler404(new_sock);
  }
  //此处我们只考虑短连接（每次客户端（浏览器）给服务器发送请求之前，都是新建立一个 socket 进行连接
  //对于短连接而言，只要响应写完，就可以关闭 new_sock
  //TODO
  close(new_sock);
}

void* ThreadEntry(void *arg){
  //线程入口函数，负责一次请求的完整过程
  int64_t new_sock = *(int64_t*)arg;
  printf("Thread Start!\n");
  HandlerRequest(new_sock); 
  return NULL;
}

void HttpServerStart(const char *ip, short port)
{
  //忽略掉写管道破裂信号，避免由于客户端在特殊情况下(eg: 等待服务器响应时间过长)而主动断开连接，从而
  //导致服务器向一个已经关闭的socket信道写数据，导致引发写管道破裂信号强制关闭HTTP服务器进程
  signal(SIGPIPE, SIG_IGN);
  //创建 tcp socket
  int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  if(listen_sock < 0){
    perror("socket");;
    return;
  }
  //设置 REUSEADDR，将端口设置为可重用式，解决短连接主动关闭 socket 出现大量 time_wait 状态的问题
  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  //绑定端口号 
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(ip);
  addr.sin_port = htons(port);
  int ret = bind(listen_sock, (sockaddr*)&addr, sizeof(addr));
  if(ret < 0){
    perror("bind");
    return;
  }
  ret = listen(listen_sock, 5);
  if(ret < 0){
    perror("listen");
    return;
  }
  printf("Server Start!\n");
  while(1){
    sockaddr_in peer;
    socklen_t len = sizeof(peer);
    int64_t new_sock = accept(listen_sock, (sockaddr*)&peer, &len);
    if(new_sock < 0){
      perror("accept");
      continue;
    }
    pthread_t tid;
    pthread_create(&tid, NULL, ThreadEntry, (void*)&new_sock); 
    pthread_detach(tid);
  }
}

int main(int argc, char* argv[]) {
  if(argc != 3) {
    printf("Usage: ./http_server.c [IP] [port]");
    return 1;
  }
  HttpServerStart(argv[1], atoi(argv[2]));

  return 0;
}

