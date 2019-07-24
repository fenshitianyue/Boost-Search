#include <base/base.h>
#include <sofa/pbrpc/pbrpc.h>
#include <ctemplate/template.h>
#include "../../common/util.hpp"
#include "server.pb.h"

DEFINE_string(server_addr, "127.0.0.1:10000", "请求的搜索服务器的地址");
DEFINE_string(template_path, "../../front/template/search_page.html", "模板文件的路径");

namespace doc_client {

typedef doc_server_proto::Request Request;
typedef doc_server_proto::Response Response;

int GetQueryString(char output[]) {
  // 1. 先从环境变量中获取到方法
  char* method = getenv("REQUEST_METHOD");
  if (method == NULL) {
    fprintf(stderr, "REQUEST_METHOD failed\n");
    return -1;
  }
  // 2. 如果是 GET 方法, 就是直接从环境变量中
  //    获取到 QUERY_STRING
  if (strcasecmp(method, "GET") == 0) {
    char* query_string = getenv("QUERY_STRING");
    if (query_string == NULL) {
      fprintf(stderr, "QUERY_STRING failed\n");
      return -1;
    }
    strcpy(output, query_string + 2); //去掉 1=
  } else {
    // 3. 如果是 POST 方法, 先通过环境变量获取到 CONTENT_LENGTH
    //    再从标准输入中读取 body
    char* content_length_str = getenv("CONTENT_LENGTH");
    if (content_length_str == NULL) {
      fprintf(stderr, "CONTENT_LENGTH failed\n");
      return -1;
    }
    int content_length = atoi(content_length_str);
    int i = 0;  // 表示当前已经往  output 中写了多少个字符了
    for (; i < content_length; ++i) {
      read(0, &output[i], 1);
    }
    output[content_length] = '\0';
  }
  return 0;
}

void PackageRequest(Request* req) {
  // TODO:此处的 sid 的生成暂时先不考虑
  req->set_sid(0);
  req->set_timestamp(common::TimeUtil::TimeStamp());
  // 此处的查询词, 后面要根据 CGI 的方式从环境变量
  // 中获取到这个查询词
  char buf[1024] = {0};
  GetQueryString(buf);
  // 经过刚才这个函数的调用, buf 中就包含了
  // query=filesystem 字符串
  char query[1024] = {0};
  sscanf(buf, "query=%s", query);
  req->set_query(query);
}

void Search(const Request& req, Response* resp) {
  // 此函数需要调用 RPC 框架中的服务器所提供的
  // Search 函数完成相加
  using namespace sofa::pbrpc;
  // 此处又涉及到 RPC 框架中的一些相关概念
  // 1. 先定义一个 RPC client 对象
  RpcClient client;
  // 2. 再定义一个 RpcChannel 对象, 描述了一个连接
  RpcChannel channel(&client, fLS::FLAGS_server_addr);
  // 3. 再定义一个 DocServerAPI_Stub. 描述的是调用服务器
  //    中的哪个函数
  doc_server_proto::DocServerAPI_Stub stub(&channel);
  // 4. 再定义一个 ctrl 对象, 用于网络控制的对象
  RpcController ctrl;
  ctrl.SetTimeout(3000);
  // 5. 万事俱备, 调用远程函数了, 此处在客户端调用的本地
  //    函数就相当于调用到远端服务器的函数了
  stub.Search(&ctrl, &req, resp, NULL);
  // 6. 是否远程调用成功
  if (ctrl.Failed()) {
    std::cerr << "PRC Search failed\n";
  } else {
    std::cerr << "PRC Search OK\n";
  }
}

void ParseResponse(const Response& resp) {
  // 返回的响应结果是一个 HTML 
  // std::cout << resp.Utf8DebugString() << "\n";
  // 此处使用 ctemplate 完成页面的构造.
  // 目的为了 html 所描述的界面和 cpp 所描述的逻辑拆分开
  ctemplate::TemplateDictionary dict("SearchPage");
  for (int i = 0; i < resp.item_size(); ++i) {
    ctemplate::TemplateDictionary* table_dict = dict.AddSectionDictionary("item");
    table_dict->SetValue("title", resp.item(i).title());
    table_dict->SetValue("desc", resp.item(i).desc());
    table_dict->SetValue("jump_url", resp.item(i).jump_url());
    table_dict->SetValue("show_url", resp.item(i).show_url());
  }
  // 把模板文件加载起来
  ctemplate::Template* tpl = ctemplate::Template::GetTemplate(fLS::FLAGS_template_path, ctemplate::DO_NOT_STRIP);
  // 对模板进行替换
  std::string html;
  tpl->Expand(&html, &dict);
  std::cout << html.data(); //TODO：这里需要和http框架对接
  return;
}

// 此函数为客户端请求服务器的入口函数
void CallServer() {
  // 0. 用户输入数据
  // 1. 构造请求并发送给服务器
  Request req;
  Response resp;
  PackageRequest(&req);
  // 2. 从服务器获取到响应并解析
  Search(req, &resp);
  // 3. 解析响应把结果输出出来
  ParseResponse(resp);
  return;
}

}  // end doc_client

int main(int argc, char* argv[]) {
  base::InitApp(argc, argv);
  doc_client::CallServer();
  return 0;
}

