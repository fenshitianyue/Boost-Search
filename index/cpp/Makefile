PROTOC=~/third_part/bin/protoc
FLAG=-std=c++11 -I ~/third_part/include -L ~/third_part/lib\
		 -lpthread -lprotobuf -lgflags -lglog -g

.PHONY:all
all:index_builder index_dump

index_builder:index_builder.cc libindex.a
	g++ index_builder.cc ./libindex.a $(FLAG) -o $@
	cp -f $@ ../bin/

index_dump:index_dump.cc libindex.a
	g++ index_dump.cc ./libindex.a $(FLAG) -o $@
	cp -f $@ ../bin/

libindex.a:index.cc index.pb.cc
	g++ -c index.cc -o index.o $(FLAG)
	g++ -c index.pb.cc -o index.pb.o $(FLAG)
	ar -rc libindex.a index.pb.o index.o

index.pb.cc:index.proto
	$(PROTOC) index.proto --cpp_out=.

.PHONY:clean
clean:
	rm -f index_dump index_builder *.o libindex.a *.pb.cc *.pb.h


