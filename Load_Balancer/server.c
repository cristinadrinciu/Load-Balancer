/* Copyright 2023 Drinciu Cristina */
#include <stdlib.h>
#include <string.h>
#include "server.h"

server_memory *init_server_memory()
{
	server_memory *server = malloc(sizeof(server_memory));
	DIE(!server, "Error.\n");
	server->server_ht = ht_create(HMAX, hash_function_string,
								  compare_function_strings);
	return server;
}

void server_store(server_memory *server, char *key, char *value)
{
	ht_put(server->server_ht, key, strlen(key) + 1, value, strlen(value) + 1);
}

char *server_retrieve(server_memory *server, char *key)
{
	char *value = ht_get(server->server_ht, key);
	return value;
}

void server_remove(server_memory *server, char *key)
{
	ht_remove_entry(server->server_ht, key);
}

void free_server_memory(server_memory *server)
{
	ht_free(server->server_ht);
	free(server);
	server = NULL;
}
