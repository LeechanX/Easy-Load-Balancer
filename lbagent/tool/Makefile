TARGET = getroute
CXX = g++
CFLAGS = -g -O2 -Wall

$(TARGET):
	$(CXX) $(CFLAGS) -o $(TARGET) $(TARGET).cc -I../api/cpp/elbApi/ ../api/cpp/elbApi/libelbapi.a ../common/protobuf/lib/libprotobuf.a
