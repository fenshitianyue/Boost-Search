#include <base/base.h>
#include "index.h"

DEFINE_string(index_path, "../data/output/index_file",
              "索引文件的路径");
DEFINE_string(forward_dump_file, "../data/tmp/forward_dump",
              "正排的反解文件");
DEFINE_string(inverted_dump_file, "../data/tmp/inverted_dump",
              "倒排的反解文件");

int main(int argc, char* argv[]) {
  base::InitApp(argc, argv);
  doc_index::Index* index = doc_index::Index::Instance();
  CHECK(index->Load(fLS::FLAGS_index_path));
  CHECK(index->Dump(fLS::FLAGS_forward_dump_file,
        fLS::FLAGS_inverted_dump_file));
  return 0;
}
