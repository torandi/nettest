OBJS = client.o client_main.o render.o texture.o resource.o player.o logic.o render_object.o level.o sha1.o network_lib.o socket.o protocol.o server.o
SPRITES = dispencer.png tail.png
CFLAGS += -Wall -g 
#LDFLAGS += 

all: nettest
 
nettest:  $(OBJS) $(SPRITES)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -rf *.o *.d nettest

%.o : %.cpp
	@$(CXX) -MM $(CFLAGS) $< > $*.d
	$(CXX) $(CFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

test_network: sha1.o network_lib.o network_test.o protocol.o socket.o
	$(CXX) network_lib.o sha1.o protocol.o socket.o network_test.o $(LDFLAGS) -o $@
	
