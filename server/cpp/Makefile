PROTOC=~/third_part/bin/protoc

FLAG=-g -std=c++11 -I ~/third_part/include -L ~/third_part/lib\
		 -L ../../index/cpp \
		 -lsofa-pbrpc -lprotobuf -lglog -lgflags -lpthread\
		 -lz -lsnappy

server:server_main.cc server.pb.cc doc_searcher.cc ../../index/cpp/libindex.a
	g++ $^  -o $@ $(FLAG)
	mv -f $@ ../bin

server.pb.cc:server.proto
	$(PROTOC) server.proto --cpp_out=.

.PHONY:clean
clean:
	rm server.pb.*
