#pragma once

#include <vector>
#include <unordered_map>
#include <cppjieba/Jieba.hpp>
#include <utility>
#include "index.pb.h"
#include "../../common/util.hpp"

namespace doc_index {

// 定义一些类型
typedef doc_index_proto::DocInfo DocInfo;
typedef doc_index_proto::Weight Weight;
typedef std::vector<DocInfo> ForwardIndex;
typedef std::vector<Weight> InvertedList;  // 倒排拉链
typedef std::unordered_map<std::string, InvertedList> InvertedIndex;

struct WordCnt {
  int title_cnt;
  int content_cnt;
  int first_pos;  // 记录了这个词在正文中第一次出现的位置.
                  // 为了方便后面构造描述信息
  // 此处把 first_pos 初始化为 -1 , 为了后面判定 该词 到底在正文中
  // 是否出现过
  WordCnt() : title_cnt(0), content_cnt(0), first_pos(-1) {}
};

typedef std::unordered_map<std::string, WordCnt> WordCntMap;

// 索引模块核心类. 和索引相关的全部操作都包含在这个类中
// a) 构建, raw_input 中的内容进行解析在内存中构造
//    出索引结构(hash)
// b) 保存, 把内存中的索引结构进行序列化, 存到文件中
//    (序列化就依赖了刚才的 index.proto) 制作索引的可执行
//    程序来调用保存
// c) 加载, 把磁盘上的索引文件读取出来, 反序列化, 生成
//    内存中的索引结构拱搜索服务器使用
// d) 反解, 内存中的索引结果按照一定的格式打印出来, 方便
//    测试
// e) 查正排, 给定文档id, 获取到文档的详细信息
// f) 查倒排, 给定关键词, 获取到和关键词相关的文档列表
class Index {
public:
  Index();

  static Index* Instance() {
    if (inst_ == NULL) {
      inst_ = new Index();
    }
    return inst_;
  }

  // 从 raw_input 文件中读数据, 在内存中构建成索引结构
  bool Build(const std::string& input_path);

  // 把内存中的索引数据保存到磁盘上
  bool Save(const std::string& output_path);

  // 把磁盘上的文件加载到内存的索引结构中
  bool Load(const std::string& index_path);

  // 调试用的接口, 把内存中的索引数据按照一定的格式打印到文件中
  bool Dump(const std::string& forward_dump_path, const std::string& inverted_dump_path);

  // 根据 doc_id 获取到 文档详细信息
  const DocInfo* GetDocInfo(uint64_t doc_id) const;

  // 根据关键词获取到 倒排拉链(包含了一组doc_id)
  const InvertedList* GetInvertedList(const std::string& key) const;

  // 此处为了方便 服务器 进行分词, 再提供一个函数
  void CutWordWithoutStopWord(const std::string& query, std::vector<std::string>* words);
  
private:
  ForwardIndex forward_index_;
  InvertedIndex inverted_index_;
  cppjieba::Jieba jieba_;
  common::DictUtil stop_word_dict_;

  static Index* inst_;

  // 以下函数为内部使用的函数
  const DocInfo* BuildForward(const std::string& line);
  void BuildInverted(const DocInfo& doc_info);
  void SortInverted();
  void SplitTitle(const std::string& title, DocInfo* doc_info);
  void SplitContent(const std::string& content,
                    DocInfo* doc_info);
  int CalcWeight(int title_cnt, int content_cnt);
  static bool CmpWeight(const Weight& w1, const Weight& w2);
  bool ConvertToProto(std::string* proto_data);
  bool ConvertFromProto(const std::string& proto_data);
};

}  // end doc_index
