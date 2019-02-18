all: bin msgq_server msgq_client

bin:
	mkdir bin

msgq_server: ./src/msgq_server.c
	gcc ./src/msgq_server.c -o ./bin/msgq_server

msgq_client: ./src/msgq_client.c
	gcc ./src/msgq_client.c -o ./bin/msgq_client
