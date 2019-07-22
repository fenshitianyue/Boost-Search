#include <iostream>
#include "hello.pb.h"

int main() {
  // 1. 基于 Hello 类完成序列化
  Hello hello;
  hello.set_name("hehe");
  hello.set_score(100);
  std::string buf;
  hello.SerializeToString(&buf);
  std::cout << buf << std::endl;
  //分隔
  std::cout << "----------------------" << std::endl;
  Hello hello_result;
  hello_result.ParseFromString(buf);
  std::cout << hello_result.name() << std::endl
            << hello_result.score() << std::endl;
  return 0;
}
