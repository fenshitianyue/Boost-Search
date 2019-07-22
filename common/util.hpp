#pragma once 
#include <string>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <boost/algorithm/string.hpp>
#include <sys/time.h>

namespace common {

class StringUtil {
public:
  static void Split(const std::string& input, std::vector<std::string>* output, const std::string& split_char) {
    boost::split(*output, input, boost::is_any_of(split_char), boost::token_compress_off);
  }

  static int32_t FindSentenceBeg(const std::string& content, int32_t first_pos) {
    for(auto i = first_pos; i >= 0; --i) {
      if (content[i] == ';' || content[i] == ',' || content[i] == '?' || content[i] == '!' || (content[i] == '.' 
          && content[i + 1] == ' ')) {
        return i + 1;
      }
    }
    // 如果往前找的过程中没有找到句子的分割符, 就认为当前所在的句子
    // 就是文章的第一句话. 也就可以用 0 表示句子的开始
    return 0;
  }
};

class DictUtil {
public:
  bool Load(const std::string& path) {
    std::ifstream file(path.c_str());
    if(!file.is_open()){
      return false;
    }
    std::string line; 
    while (std::getline(file, line)) {
      set_.insert(line);
    }
    return true;
  }
  
  bool Find(const std::string& key) const {
    return set_.find(key) != set_.end();
  }
private:
  std::unordered_set<std::string> set_;
};

class FileUtil {
public:
  //把所有内容都读到 content 中
  static bool Read(const std::string& input_path, std::string* content) {
    std::ifstream file(input_path.c_str());
    if(!file.is_open()) {
      return false;
    }
    //先获取文件长度
    file.seekg(0, file.end);
    int length = file.tellg();
    file.seekg(0, file.beg);
    content->resize(length);
    file.read(const_cast<char*>(content->data()), length);
    file.close();
    return true;
  }

  //把 content 中的所有内容都写入文件
  static bool Write(const std::string& output_path, const std::string& content) {
    std::ofstream file(output_path.c_str());
    if(!file.is_open()) {
      return false;
    }
    file.write(content.data(), content.size());
    file.close();
    return true;
  }
};

class TimeUtil {
public:
  //获取到秒级时间戳
  static int64_t TimeStamp() {
    timeval tv;
    ::gettimeofday(&tv, NULL);
    return tv.tv_sec;
  }
  //获取到毫秒级时间戳
  static int64_t TimeStampMS() {
    timeval tv;
    ::gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
  }
  //获取到微秒级时间戳
  static int64_t TimeStampUS() {
    timeval tv;
    ::gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 * 1000 + tv.tv_usec;
  }
};

} // end common

