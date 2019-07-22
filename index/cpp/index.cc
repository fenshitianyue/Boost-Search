#include <fstream>
#include <base/base.h>
#include "index.h"

// 此处词典的路径一会再具体考虑
DEFINE_string(dict_path, "../../third_part/data/jieba_dict/jieba.dict.utf8", "字典路径");
DEFINE_string(hmm_path, "../../third_part/data/jieba_dict/hmm_model.utf8", "hmm 字典路径");
DEFINE_string(user_dict_path, "../../third_part/data/jieba_dict/user.dict.utf8", "用户自定制词典路径");
DEFINE_string(idf_path, "../../third_part/data/jieba_dict/idf.utf8", "idf 字典路径");
DEFINE_string(stop_word_path, "../../third_part/data/jieba_dict/stop_words.utf8", "暂停词字典路径");

namespace doc_index {

Index* Index::inst_ = NULL;

Index::Index() : jieba_(fLS::FLAGS_dict_path,
                        fLS::FLAGS_hmm_path,
                        fLS::FLAGS_user_dict_path,
                        fLS::FLAGS_idf_path,
                        fLS::FLAGS_stop_word_path) {
  CHECK(stop_word_dict_.Load(fLS::FLAGS_stop_word_path));
}

// 从 raw_input 文件中读数据, 在内存中构建成索引结构
bool Index::Build(const std::string& input_path) {
  //std::cout << "Index building..." << std::endl; //TODO:临时日志
  LOG(INFO) << "Index Build";
  // 1. 按行读取文件内容, 针对读到的每一行数据进行处理
  std::ifstream file(input_path.c_str());
  CHECK(file.is_open()) << "input_path: " << input_path;
  std::string line;
  while (std::getline(file, line)) {
    // 2. 把这一行数据制作成一个 DocInfo
    //    此处获取到的 doc_info 是为了接下来制作倒排方便
    const DocInfo* doc_info = BuildForward(line);
    // 如果构建正排失败, 就立刻让进程终止
    CHECK(doc_info != NULL);
    // 3. 更新倒排信息
    // 此函数的输出结果, 直接放到 Index::inverted_index_
    BuildInverted(*doc_info);
  }
  // 4. 处理完所有的文档之后, 针对所有的倒排拉链进行排序
  //    key-value 中的value进行排序. 排序的依据按照权重
  //    降序排序
  SortInverted();
  file.close();
  LOG(INFO) << "Index Build Done!!!";
  return true;
}

const DocInfo* Index::BuildForward(const std::string& line) {
  // 1. 先对 line 进行字符串切分
  std::vector<std::string> tokens;
  // 当前的 Split 不会破坏原字符串
  common::StringUtil::Split(line, &tokens, "\3");
  if (tokens.size() != 3) {
    LOG(FATAL) << "line split not 3 tokens! tokens.size()="
               << tokens.size();
    return NULL;
  }
  // 2. 构造一个 DocInfo 结构, 把切分的结果赋值到 DocInfo
  //    除了分词结果之外都能进行赋值
  DocInfo doc_info;
  doc_info.set_id(forward_index_.size());
  doc_info.set_title(tokens[1]);
  doc_info.set_content(tokens[2]);
  doc_info.set_jump_url(tokens[0]);
  // 此处把 show_url 直接填成和 jump_url 一致.
  // 实际上真实的搜索引擎中通常是 show_url 只包含 jump_url
  // 的域名. 但是此处我们先不这样处理
  doc_info.set_show_url(doc_info.jump_url());
  // 3. 对标题和正文进行分词, 把分词结果保存到 DocInfo 中
  //    此处 doc_info 是输出参数, 用指针的方式传进去
  SplitTitle(tokens[1], &doc_info);
  SplitContent(tokens[2], &doc_info);
  // 4. 把这个 DocInfo 插入到正排索引中
  forward_index_.emplace_back(std::move(doc_info)); //TODO:以右值的方式插入临时对象更快一些
  return &forward_index_.back();
}

void Index::SplitTitle(const std::string& title, DocInfo* doc_info) {
  std::vector<cppjieba::Word> words;
  // 要调用 cppjieba 进行分词, 需要先创建一个 jieba 对象
  jieba_.CutForSearch(title, words);
  // words 里面包含的分词结果, 每个结果包含一个 offset.
  // offset 表示的是当前词在文档中的起始位置的下标.
  // 而实际上我们需要知道的是一个前闭后开区间.
  if (words.size() < 1) {
    // 错误处理
    LOG(FATAL) << "SplitTitle failed! title = " << title;
    return;
  }
  for (size_t i = 0; i < words.size(); ++i) {
    auto* token = doc_info->add_title_token();
    token->set_beg(words[i].offset);
    if (i + 1 < words.size()) {
      // i 不是最后一个元素
      token->set_end(words[i + 1].offset);
    } else {
      // i 是最后一个元素
      token->set_end(title.size());
    }
  }  // end for
  return;
}

void Index::SplitContent(const std::string& content,
    DocInfo* doc_info) {
  std::vector<cppjieba::Word> words;
  // 要调用 cppjieba 进行分词, 需要先创建一个 jieba 对象
  jieba_.CutForSearch(content, words);
  // words 里面包含的分词结果, 每个结果包含一个 offset.
  // offset 表示的是当前词在文档中的起始位置的下标.
  // 而实际上我们需要知道的是一个前闭后开区间.
  if (words.size() <= 1) {
    // 错误处理
    LOG(FATAL) << "SplitContent failed!";
    return;
  }
  for (size_t i = 0; i < words.size(); ++i) {
    auto* token = doc_info->add_content_token();
    token->set_beg(words[i].offset);
    if (i + 1 < words.size()) {
      // i 不是最后一个元素
      token->set_end(words[i + 1].offset);
    } else {
      // i 是最后一个元素
      token->set_end(content.size());
    }
  }  // end for
  return;
}

void Index::BuildInverted(const DocInfo& doc_info) {
  WordCntMap word_cnt_map;
  // 1. 统计 title 中每个词出现的个数
  for (int i = 0; i < doc_info.title_token_size(); ++i) {
    //获取当前分词
    const auto& token = doc_info.title_token(i); 
    std::string word = doc_info.title().substr(token.beg(), token.end() - token.beg());
    // 假设文档中, Hello, hello 应该算作一个词. 大小写不敏感
    boost::to_lower(word);
    // 干掉暂停词
    if (stop_word_dict_.Find(word)) {
      continue;
    }
    ++word_cnt_map[word].title_cnt;
  }
  // 2. 统计 content 中每个词出现的个数
  //    此时得到了一个 hash 表. hash 表中的key就是
  //    关键词(切分结果)
  //    hash 表中的value就是一个结构体, 结构体里面包含了
  //    该词在标题中出现的次数和该词在正文中出现的次数
  for (int i = 0; i < doc_info.content_token_size(); ++i) {
    const auto& token = doc_info.content_token(i);
    std::string word = doc_info.content().substr(token.beg(), token.end() - token.beg());
    boost::to_lower(word);
    if (stop_word_dict_.Find(word)) {
      continue;
    }
    ++word_cnt_map[word].content_cnt;
    // 记录下词在正文中的第一次出现的位置
    if (1 == word_cnt_map[word].content_cnt) {
      word_cnt_map[word].first_pos = token.beg();
    }
  }
  // 3. 根据个数的统计结果, 更新到倒排索引之中
  //    遍历刚才的这个hash表, 拿着 key 去倒排索引中去查
  //    如果倒排索引中不存在这个词, 就新增一项
  //    如果倒排索引中已经存在这个词, 就根据当前构造好的
  //    Weight结构添加到倒排索引中对应的倒排拉链中
  for (const auto& word_pair : word_cnt_map) {
    Weight weight;
    weight.set_doc_id(doc_info.id());
    weight.set_weight(CalcWeight(word_pair.second.title_cnt, word_pair.second.content_cnt));
    weight.set_first_pos(word_pair.second.first_pos);
    // 先获取到当前词对应的倒排拉链
    InvertedList& inverted_list = inverted_index_[word_pair.first];
    inverted_list.emplace_back(std::move(weight)); //TODO：临时对象使用右值插入
  }
  return;
}

int Index::CalcWeight(int title_cnt, int content_cnt) {
  // 权重我们使用一种简单粗暴的方式来进行计算
  return 10 * title_cnt + content_cnt;
}

void Index::SortInverted() {
  // 把所有的倒排拉链都按照 weight 降序排序
  for (auto& inverted_pair : inverted_index_) {
    InvertedList& inverted_list = inverted_pair.second;
    std::sort(inverted_list.begin(), inverted_list.end(), CmpWeight);
  }
}

bool Index::CmpWeight(const Weight& w1, const Weight& w2) {
  return w1.weight() > w2.weight();
}

// 把内存中的索引数据保存到磁盘上
bool Index::Save(const std::string& output_path) {
  //std::cout << "Index Saved..." << std::endl; //TODO:临时日志
  LOG(INFO) << "Index Save";
  // 1. 把内存中的索引结构序列化成字符串
  std::string proto_data;
  CHECK(ConvertToProto(&proto_data));
  // 2. 把序列化得到的字符串写到文件中
  CHECK(common::FileUtil::Write(output_path, proto_data));
  LOG(INFO) << "Index Save Done";
  return true;
}

bool Index::ConvertToProto(std::string* proto_data) {
  doc_index_proto::Index index;
  // 需要把内存中的数据设置到 index 中
  // 1. 设置正排
  for (const auto& doc_info : forward_index_) {
    auto* proto_doc_info = index.add_forward_index();
    *proto_doc_info = doc_info;
  }
  // 2. 设置倒排
  for (const auto& inverted_pair : inverted_index_) {
    auto* kwd_info = index.add_inverted_index();
    kwd_info->set_key(inverted_pair.first);
    for (const auto& weight : inverted_pair.second) {
      auto* proto_weight = kwd_info->add_doc_list();
      *proto_weight = weight;
    }
  }
  index.SerializeToString(proto_data);
  return true;
}

// 把磁盘上的文件加载到内存的索引结构中
bool Index::Load(const std::string& index_path) {
  //std::cout << "Index loading..." << std::endl; //TODO:临时日志
  LOG(INFO) << "Index Load";
  // 1. 从磁盘上把索引文件读到内存中
  std::string proto_data;
  CHECK(common::FileUtil::Read(index_path, &proto_data));
  // 2. 进行反序列化, 转成内存的索引结构
  CHECK(ConvertFromProto(proto_data));
  LOG(INFO) << "Index Load Done";
  return true;
}

bool Index::ConvertFromProto(const std::string& proto_data) {
  // 1. 对索引文件内容进行反序列化
  doc_index_proto::Index index;
  index.ParseFromString(proto_data);
  // 2. 把正排索引数据放到内存中
  for (int i = 0; i < index.forward_index_size(); ++i) {
    const auto& doc_info = index.forward_index(i);
    forward_index_.push_back(doc_info); //TODO:考虑使用右值插入提高效率
  }
  // 3. 把倒排索引数据放到内存中
  for (int i = 0; i < index.inverted_index_size(); ++i) {
    const auto& kwd_info = index.inverted_index(i);
    InvertedList& inverted_list = inverted_index_[kwd_info.key()];
    for (int j = 0; j < kwd_info.doc_list_size(); ++j) {
      const auto& weight = kwd_info.doc_list(j);
      inverted_list.push_back(weight); //TODO:考虑使用右值插入提高效率
    }
  }
  return true;
}

// 调试用的接口, 把内存中的索引数据按照一定的格式打印到
// 文件中
bool Index::Dump(const std::string& forward_dump_path, const std::string& inverted_dump_path) {
  //std::cout << "Index dumping..." << std::endl; //TODO：临时日志
  LOG(INFO) << "Index Dump";
  // 1. 处理正排
  std::ofstream forward_dump_file(forward_dump_path.c_str());
  CHECK(forward_dump_file.is_open());
  for (size_t i = 0; i < forward_index_.size(); ++i) {
    const DocInfo& doc_info = forward_index_[i];
    forward_dump_file << doc_info.Utf8DebugString()
                      << "=================";
  }
  forward_dump_file.close();
  // 2. 处理倒排
  std::ofstream inverted_dump_file(inverted_dump_path.c_str());
  CHECK(inverted_dump_file.is_open());
  for (const auto& inverted_pair : inverted_index_) {
    inverted_dump_file << inverted_pair.first << "\n";
    for (const auto& weight : inverted_pair.second) {
      inverted_dump_file << weight.Utf8DebugString();
    }
    inverted_dump_file << "==================";
  }
  inverted_dump_file.close();
  LOG(INFO) << "Index Dump Done";
  return true;
}

// 根据 doc_id 获取到 文档详细信息
const DocInfo* Index::GetDocInfo(uint64_t doc_id) const {
  if (doc_id >= forward_index_.size()) {
    return NULL;
  }
  return &forward_index_[doc_id];
}

// 根据关键词获取到 倒排拉链(包含了一组doc_id)
const InvertedList* Index::GetInvertedList( const std::string& key) const {
  auto it = inverted_index_.find(key);
  if (it == inverted_index_.end()) {
    return NULL;
  }
  return &(it->second);
}

// 需要把所有的暂停词从分词结果中过滤掉
void Index::CutWordWithoutStopWord(const std::string& query, std::vector<std::string>* words) {
  words->clear();
  std::vector<std::string> tmp;
  jieba_.CutForSearch(query, tmp);
  for (std::string& token : tmp) {
    // 判定暂停词逻辑和大小写无关
    boost::to_lower(token);
    if (stop_word_dict_.Find(token)) {
      continue;
    }
    words->push_back(token); //TODO：考虑使用右值插入提高效率
  }
}
}  // end doc_index
