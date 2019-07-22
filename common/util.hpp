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

} // end common
