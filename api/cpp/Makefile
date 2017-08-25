TARGET = example
CXX = g++
CFLAGS = -g -O2 -Wall

$(TARGET):
	$(CXX) $(CFLAGS) -o $(TARGET) $(TARGET).cc -IelbApi elbApi/libelbapi.a ../../common/protobuf/lib/libprotobuf.a
