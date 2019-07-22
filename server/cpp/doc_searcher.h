#pragma once

#include "server.pb.h"
#include "../../index/cpp/index.h"

namespace doc_server {

typedef doc_server_proto::Request Request;
typedef doc_server_proto::Response Response;
typedef doc_index_proto::Weight Weight;
typedef doc_index::Index Index;

// 请求的上下文信息
struct Context {
  const Request* req;
  Response* resp;
  // 保存分词结果
  std::vector<std::string> words;
  // 保存触发出的倒排拉链的结果集合
  std::vector<const Weight*> all_query_chain;

  Context(const Request* request, Response* response)
    : req(request), resp(response) {  }
};

// 这个类是完成搜索的核心类
class DocSearcher {
public:
  // 搜索流程的入口函数
  bool Search(const Request& req, Response* resp);

private:
  // 对查询词进行分词
  bool CutQuery(Context* context);
  // 根据查询词结果进行触发
  bool Retrieve(Context* context);
  // 根据触发的结果进行排序
  bool Rank(Context* context);
  // 根据排序的结构拼装成响应
  bool PackageResponse(Context* context);
  // 打印请求日志
  bool Log(Context* context);
  // 排序需要的比较函数
  static bool CmpWeightPtr(const Weight* w1, const Weight* w2);
  // 生成描述信息
  std::string GenDesc(int first_pos, const std::string& content);
  // 替换 html 中的转义字符
  void ReplaceEscape(std::string* desc);
};
}  // end doc_server
