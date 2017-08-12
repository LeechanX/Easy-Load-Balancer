CXX = g++
CFLAGS = -g -O2 -Wall

all:
	protoc elb.proto --cpp_out=.

clean:
	-rm *.cc *.h
