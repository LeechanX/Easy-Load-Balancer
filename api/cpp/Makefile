TARGET = example
CXX = g++
CFLAGS = -g -O2 -Wall

all:
	$(CXX) $(CFLAGS) -o example example.cc -IelbApi elbApi/libelbapi.a ../../common/protobuf/lib/libprotobuf.a
	$(CXX) $(CFLAGS) -o qpstest qpstest.cc -IelbApi elbApi/libelbapi.a ../../common/protobuf/lib/libprotobuf.a -lpthread
