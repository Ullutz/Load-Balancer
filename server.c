/* Copyright 2023 <> */
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "list.h"

unsigned int hash_function(void *a) {
    unsigned char *puchar_a = (unsigned char *)a;
    unsigned int hash = 5381;
    int c;

    while ((c = *puchar_a++))
        hash = ((hash << 5u) + hash) + c;

    return hash;
}

server_memory *init_server_memory()
{
	//creez serverul si ii aloc memorie
	server_memory *server = (server_memory *) malloc(sizeof(server_memory));
	DIE(!server, "Server alloc failed");

	//aloc memorie pentru hashtable
	server->ht = ht_create(HMAX, hash_function, compare_function_strings,
													key_val_free_function);
	return server;
}

void server_store(server_memory *server, char *key, char *value) {
	int key_size = strlen(key);
	int val_size = strlen(value);

	//pun in server noua cheie
	ht_put(server->ht, key, key_size, value, val_size);
}

char *server_retrieve(server_memory *server, char *key) {
	char *value = NULL;

	//folosesc functia din lab pentru a gasi valoarea, dar pentru ca
	//functia este facuta pentru void *, fac si o conversie.
	value = (char *)ht_get(server->ht, key);
	return value;
}

void server_remove(server_memory *server, char *key) {
	ht_remove_entry(server->ht, key);
}

void free_server_memory(server_memory *server) {
	ht_free(server->ht);
	free(server);
}
