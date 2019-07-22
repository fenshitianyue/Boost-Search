PROTOC=~/third_part/bin/protoc

main:main.cc hello.pb.cc
	g++ $^ -o $@ -I ~/third_part/include -L ~/third_part/lib -lprotobuf

hello.pb.cc:hello.proto
	$(PROTOC) hello.proto --cpp_out=.

.PHONY:clean
clean:
	rm main

