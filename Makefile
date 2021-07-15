out: server.o dbserver.o
		g++ server.o -o out/server -lpthread -lpaho-mqttpp3 -lpaho-mqtt3as
		g++ dbserver.o -o out/dbserver -lpthread -lpqxx  -lpaho-mqttpp3 -lpaho-mqtt3as
	
server.o: ./src/server/server.cpp
		g++ -c -g -I $(ARGS) ./src/server/server.cpp 

dbserver.o: ./src/dbserver/dbserver.cpp
		g++ -c -g -I $(ARGS) ./src/dbserver/dbserver.cpp

clean:
		rm *.o
		
