TARGET = example
CXX = g++
CFLAGS = -g -O2 -Wall

all:
	$(CXX) $(CFLAGS) -o example example.cc -I../elbApi ../lib/libelbapi.a ../../../common/protobuf/lib/libprotobuf.a
	$(CXX) $(CFLAGS) -o qpstest qpstest.cc -I../elbApi ../lib/libelbapi.a ../../../common/protobuf/lib/libprotobuf.a -lpthread
	$(CXX) $(CFLAGS) -o timotest timotest.cc -I../elbApi ../lib/libelbapi.a ../../../common/protobuf/lib/libprotobuf.a -lpthread
clean:
	rm -f example qpstest timotest
