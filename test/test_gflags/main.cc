#include <gflags/gflags.h>
#include <iostream>

DEFINE_string(ip, "127.0.0.1", "ip地址");
DEFINE_int32(port, 8080, "端口号");
DEFINE_bool(use_tcp, true, "是否使用 TCP 协议");


int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::cout << "ip: " << FLAGS_ip << std::endl;
  std::cout << "port: " << FLAGS_port << std::endl;
  std::cout << "use_tcp: " << std::boolalpha << FLAGS_use_tcp << std::endl;

  return 0;
}

