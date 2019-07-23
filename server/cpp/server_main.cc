#include <base/base.h>
#include <sofa/pbrpc/pbrpc.h>
#include "../../common/util.hpp"
#include "server.pb.h"
#include "doc_searcher.h"

DEFINE_string(port, "10000", "服务器端口号");
DEFINE_string(index_path, "../index/index_file", "索引文件的路径");

namespace doc_server {

typedef doc_server_proto::Request Request;
typedef doc_server_proto::Response Response;

class DocServerAPIImpl : public doc_server_proto::DocServerAPI {
public:
  // 此函数是真正在服务器端完成计算的函数
  void Search(::google::protobuf::RpcController* controller, const Request* req,
              Response* resp, ::google::protobuf::Closure* done) {
    (void) controller;

    // resp->set_sid(req->sid()); //TODO
    // resp->set_timestamp(common::TimeUtil::TimeStamp()); //TODO
    // resp->set_err_code(0);

    DocSearcher searcher;
    searcher.Search(*req, resp);

    done->Run();
  }
};

}  // end doc_server

int main(int argc, char* argv[]) {
  base::InitApp(argc, argv); 
  using namespace sofa::pbrpc;
  // 0. 索引模块的加载和初始化
  doc_index::Index* index = doc_index::Index::Instance();
  CHECK(index->Load(fLS::FLAGS_index_path));
  LOG(INFO) << "Index Load Done!";
  std::cout << "Index Load Done!";
  
  // 1. 定义一个 RpcServerOptions 对象
  //    这个对象描述了 RPC 服务器一些相关选项
  //    主要是为了定义线程池中线程的个数
  RpcServerOptions option;
  option.work_thread_num = 4;
  // 2. 定义一个 RpcServer 对象(和ip端口号关联到一起)
  RpcServer server(option);
  CHECK(server.Start("0.0.0.0:" + fLS::FLAGS_port));
  // 3. 定义一个 DocServerAPIImpl, 并且注册到 RpcServer 对象中
  doc_server::DocServerAPIImpl* service_impl = new doc_server::DocServerAPIImpl();
  server.RegisterService(service_impl);
  LOG(INFO) << "server start";
  // 4. 让 RpcServer 对象开始执行
  server.Run();
  return 0;
}
