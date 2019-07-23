PROTOC=~/third_part/bin/protoc
FLAG=-g -std=c++11 -I ~/third_part/include -L ~/third_part/lib\
		 -lsofa-pbrpc -lgflags -lglog -lprotobuf -lpthread\
		 -lz -lsnappy -lctemplate

client:client_main.cc server.pb.cc
	g++ $^ -o $@ $(FLAG)
	mv -f $@ ../bin

server.pb.cc:server.proto
	$(PROTOC) server.proto --cpp_out=.

.PHONY:clean
clean:
	rm server.pb.*
