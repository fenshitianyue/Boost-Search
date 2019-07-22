#include "doc_searcher.h"
#include <base/base.h>

DEFINE_int32(desc_max_size, 160, "描述的最大长度");

namespace doc_server {

bool DocSearcher::Search(const Request& req, Response* resp) {
  Context context(&req, resp);
  // 1. 对查询词进行分词
  CutQuery(&context);
  // 2. 根据分词结果进行触发
  Retrieve(&context);
  // 3. 根据触发结果进行排序
  Rank(&context);
  // 4. 根据排序结果进行包装响应
  PackageResponse(&context);
  // 5. 记录处理日志
  Log(&context);
  return true;
}

bool DocSearcher::CutQuery(Context* context) {
  // 使用 Jieba 分词来切分, 需要去掉暂停词
  Index* index = Index::Instance();
  index->CutWordWithoutStopWord(context->req->query(), &context->words);
  LOG(INFO) << "CutQuery Done! sid=" << context->req->sid();
  return true;
}

bool DocSearcher::Retrieve(Context* context) {
  Index* index = Index::Instance();
  // 根据分词的结果, 去从索引中找到所有的倒排拉链
  for (const auto& word : context->words) {
    const doc_index::InvertedList* inverted_list = index->GetInvertedList(word);
    if (inverted_list == NULL) {
      // 针对该分词结果, 没找到倒排拉链
      continue;
    }
    for (size_t i = 0; i < inverted_list->size(); ++i) {
      const auto& weight = (*inverted_list)[i];
      context->all_query_chain.push_back(&weight);
    }
  }
  // 当此循环结束之后, 所有分词结果对应的倒排信息就都放到
  // all_query_chain vector 之中了
  return true;
}

bool DocSearcher::Rank(Context* context) {
  // 虽然之前在制作索引过程中已经对每个倒排拉链都排过序, 
  // 但是 all_query_chain 这是一个多个倒排合并得到的最终结果.
  // 针对这个结果, 还需要进行一个统一的排序
  // 此处排序规则是要根据所有的 Weight 中的权重进行降序排序
  std::sort(context->all_query_chain.begin(), context->all_query_chain.end(), CmpWeightPtr);
  return true;
}

bool DocSearcher::CmpWeightPtr(const Weight* w1, const Weight* w2) {
  return w1->weight() > w2->weight();
}

bool DocSearcher::PackageResponse(Context* context) {
  // 构造出最终的 Response 结构
  // all_query_chain 这是这个函数的输入数据. 
  // 根据这里的文档 id, 查找到对应的相关属性(从正排中查找)
  Index* index = Index::Instance();
  const Request* req = context->req;
  Response* resp = context->resp;
  resp->set_sid(req->sid());
  resp->set_timestamp(common::TimeUtil::TimeStamp());
  resp->set_err_code(0);
  for (const auto* weight : context->all_query_chain) {
    // 查正排, 根据 doc_id, 获取到文档的属性
    const auto* doc_info = index->GetDocInfo(weight->doc_id());
    // doc_info 数目和返回结果中的 item 是一一对应的
    auto* item = resp->add_item();
    item->set_title(doc_info->title());
    // 此处设置描述的时候要根据正文来生成描述.
    // 描述的长度一般是比较短的. 
    // 描述中一般包含查询词中的部分关键词
    item->set_desc(GenDesc(weight->first_pos(), doc_info->content()));
    item->set_jump_url(doc_info->jump_url());
    item->set_show_url(doc_info->show_url());
  }
  return true;
}

// 此处的生成描述的策略比较灵活, 核心是为了让用户体验尽量
// 的好, 能够让用户看到描述就对文章的内容有一定的认知
std::string DocSearcher::GenDesc(int first_pos, const std::string& content) {
  // 1. 根据 first_pos 位置开始往前找, 找到这句话的开始位置
  //    (通过标点符号来区分)
  int64_t desc_beg = 0;
  // first_pos 有可能会不存在~~
  if (first_pos != -1) {
    // first_pos 存在, 该关键词在正文中出现过
    // 从 first_pos 位置开始往前查找. 如果 first_pos 不存在,
    // 就从正文开始位置作为描述的开始位置
    desc_beg = common::StringUtil::FindSentenceBeg(content, first_pos);
  }
  // 2. 从句子开始的位置, 往后去找若干个字节(描述最大长度自定义)
  std::string desc;
  if (desc_beg + FLAGS_desc_max_size >= (int32_t)content.size()) {
    // 3. 从句子开始到正文末尾不足描述最大长度 , 就取剩余这部分
    //    的字符串整体作为描述
    desc = content.substr(desc_beg);
  } else {
    // 4. 从句子开始到正文末尾超过 描述最大长度 , 就把倒数两个字节
    //    修改成 .. , 类似于省略号
    desc = content.substr(desc_beg, FLAGS_desc_max_size);
    desc[desc.size() - 1] = '.';
    desc[desc.size() - 2] = '.';
    desc[desc.size() - 3] = '.';
  }
  // 在此处需要把描述中包含的特殊字符替换成 转义字符
  // 如果不替换, 描述中如果包含这特殊字符的话, 就会导致浏览器
  // 不能正确的解析
  ReplaceEscape(&desc);
  return desc;
}

void DocSearcher::ReplaceEscape(std::string* desc) {
  boost::algorithm::replace_all(*desc, "&", "&amp;");
  boost::algorithm::replace_all(*desc, "\"", "&quot;");
  boost::algorithm::replace_all(*desc, "<", "&lt;");
  boost::algorithm::replace_all(*desc, ">", "&gt;");
}

bool DocSearcher::Log(Context* context) {
  LOG(INFO) << "[Request]" << context->req->Utf8DebugString();
  LOG(INFO) << "[Response]" << context->resp->Utf8DebugString();
  return true;
}
}  // end doc_server
