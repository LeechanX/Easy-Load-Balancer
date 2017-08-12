TARGET = bin/dnsserver.prog
CXX = g++
CFLAGS = -g -O2 -Wall

COMMON = ../common
BASE = $(COMMON)/base
BASE_H = $(BASE)/include
MYSQL = $(COMMON)/mysql-connector-c
MYSQL_H = $(MYSQL)/include
MYSQL_LIB = $(MYSQL)/lib/libmysqlclient.a
PROTOBUF = $(COMMON)/protobuf
PROTOBUF_LIB = $(PROTOBUF)/lib -lprotobuf
OTHER_LIB = -lpthread -ldl
EASYREACTOR = $(COMMON)/Easy-Reactor
EASYREACTOR_H = $(EASYREACTOR)/include
EASYREACTOR_LIB = $(EASYREACTOR)/lib -lereactor -lrt

PROTO_H = $(COMMON)/proto

INC = -Iinclude -I$(MYSQL_H) -I$(BASE_H) -I$(EASYREACTOR_H) -I$(PROTO_H)
LIB = $(MYSQL_LIB) -L$(PROTOBUF_LIB) -L$(EASYREACTOR_LIB) $(OTHER_LIB)

OBJS = $(addsuffix .o, $(basename $(wildcard src/*.cc)))
OBJS += $(PROTO_H)/elb.pb.o

$(TARGET): $(OBJS)
	-mkdir -p bin
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(INC) $(LIB)

-include $(OBJS:.o=.d) 

%.o: %.cc
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC)
	@$(CXX) -MM $*.cc $(INC) > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

.PHONY: clean

clean:
	-rm -f src/*.o src/*.d $(TARGET)
