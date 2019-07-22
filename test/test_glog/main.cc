#include <glog/logging.h>

int main(int argc, char* argv[]) {
  (void) argc;
  google::InitGoogleLogging(argv[0]);
  fLS::FLAGS_log_dir = "./log/";
  // LOG(INFO) << "hello info";
  // LOG(WARNING) << "hello warning";
  // LOG(ERROR) << "hello error";
  LOG(FATAL) << "hello fatal";
  return 0;
}

