/* Copyright 2023 Drinciu Cristina */
#include "load_balancer.h"

#include <stdlib.h>
#include <string.h>

unsigned int hash_function_servers(void *a)
{
    unsigned int uint_a = *((unsigned int *)a);

    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = (uint_a >> 16u) ^ uint_a;
    return uint_a;
}

unsigned int hash_function_key(void *a)
{
    unsigned char *puchar_a = (unsigned char *)a;
    unsigned int hash = 5381;
    int c;

    while ((c = *puchar_a++))
		hash = ((hash << 5u) + hash) + c;

    return hash;
}

load_balancer *init_load_balancer()
{
	// alloc for the load_balancer struct
    load_balancer *main = malloc(sizeof(load_balancer));
	DIE(!main, "Error.\n");

	main->nr_servers = 0;  // at first are 0 servers
	// alloc memory for the array of hash_ring elements
	main->hash_ring = malloc(sizeof(hash_ring_elem));
	DIE(!main->hash_ring, "Error.\n");
	return main;
}

void add_element(load_balancer *main, int index, hash_ring_elem new_server) {
	// resize the hash ring
	main->hash_ring = realloc(main->hash_ring,
						(main->nr_servers + 1) * sizeof(hash_ring_elem));
	DIE(!main->hash_ring, "Error.\n");

	if(index < 0) {  // at the end
		main->hash_ring[main->nr_servers] = new_server;
	} else {
		for(int i = main->nr_servers - 1; i >= index; i--)
			main->hash_ring[i + 1] = main->hash_ring[i];
		main->hash_ring[index] = new_server;
	}
	main->nr_servers++;
}

int father_server(load_balancer *main, char *key)
{
	int server_id;
	unsigned int key_hash = hash_function_key(key);
	// we verify if the hey's hash is "greater" that the hash of the last
	// server from the hash ring

	if(key_hash > hash_function_servers(
					&main->hash_ring[main->nr_servers - 1].server_id)) {
		// in this case it belongs to the first server
		server_id = main->hash_ring[0].server_id;
		return server_id;
	}

	// otherwise we have to search in the hash ring
	for(int i = 0; i < main->nr_servers; i++)
		if(key_hash < hash_function_servers(&main->hash_ring[i].server_id)) {
				server_id = main->hash_ring[i].server_id;
				break;
			}
	return server_id;
}

void redistribution(load_balancer *main, int index, hash_ring_elem new_server)
{
	// transfer the objects from the right neighbour
	server_memory *neighbour;
	// find the neighbour
	if (index == main->nr_servers - 1) {
		neighbour = main->hash_ring[0].server;
	} else {
		neighbour = main->hash_ring[index + 1].server;
	}

	int nr_buckets = neighbour->server_ht->hmax;
	if(neighbour->server_ht->size != 0 && neighbour != new_server.server) {
		for(int j = 0; j < nr_buckets; j++) {
			ll_node_t *curr = neighbour->server_ht->buckets[j]->head;
			while(curr) {
				if(father_server(main, ((info*)curr->data)->key) == new_server.server_id) {
					// if it bellongs to this dublicate
					// put it in the new server
					char *key, *value;
					key = ((info*)curr->data)->key;
					value = ((info*)curr->data)->value;
					server_store(new_server.server, key, value);
					// remove it from the original server
					curr = curr->next;
					server_remove(neighbour, key);
				} else {
					curr = curr->next;
				}
			}
		}
	}
}

void loader_add_server(load_balancer *main, int server_id)
{
	// case 1: is the first server of the loader
	if(main->nr_servers == 0) {
		// atribute the server_id
		main->hash_ring[0].server_id = server_id;

		// create the server
		main->hash_ring[0].server = init_server_memory();

		// increase the number of servers
		main->nr_servers = 1;

		// create the duplicates
		// add the first dublicate
		hash_ring_elem new_server;
		new_server.server_id = NMAX + server_id;
		new_server.server = main->hash_ring[0].server;
		if(hash_function_servers(&main->hash_ring[0].server_id) <
		   hash_function_servers(&new_server.server_id))
			add_element(main, 1, new_server);
		else
			add_element(main, 0, new_server);

		// add the second dublicate
		new_server.server_id = 2 * NMAX + server_id;
		int index = -1;
		for(int i = 0; i < main->nr_servers ; i++)
		if(hash_function_servers(&new_server.server_id) <
			hash_function_servers(&main->hash_ring[i].server_id)) {
			index = i;
			break;
			}

		add_element(main, index, new_server);
		return;
	}

	// otherwise
	// find the right place for each new server_id
	hash_ring_elem new_server;
	new_server.server_id = server_id;
	new_server.server = init_server_memory();
	// introduce each server
	for(int k = 0; k < 3; k++) {
		int index = -1;
		for(int i = 0; i < main->nr_servers; i++)
			if(hash_function_servers(&new_server.server_id) <
				hash_function_servers(&main->hash_ring[i].server_id)) {
				index = i;
				break;
			}
		add_element(main, index, new_server);
		redistribution(main, index, new_server);
		new_server.server_id = (k + 1) * NMAX  + server_id;
	}
}

void remove_element(load_balancer *main, int index)
{
	// deletes an element from the array
	for(int i = index; i < main->nr_servers - 1; i++)
		main->hash_ring[i] = main->hash_ring[i + 1];
	main->nr_servers--;
	// realloc the array
	main->hash_ring = realloc(main->hash_ring,
						sizeof(hash_ring_elem) * main->nr_servers);
	DIE(!main->hash_ring, "Error.\n");
}

void loader_remove_server(load_balancer *main, int server_id)
{
	server_memory *trash;  // to be deleted seerver
	int duplicate_id = server_id;
	for(int k = 0; k < 3; k++) {
		for(int i = 0; i < main->nr_servers; i++)
			if(main->hash_ring[i].server_id == duplicate_id) {
				trash = main->hash_ring[i].server;
				// trasfer all the objects to the right neighbour
				if(main->hash_ring[i].server->server_ht->size != 0) {
					server_memory *neighbour;
					// find the right neighbour
					if (i == main->nr_servers - 1)
						neighbour = main->hash_ring[0].server;
					else
						neighbour = main->hash_ring[i + 1].server;

					if(neighbour != main->hash_ring[i].server) {
						int nr_buckets = trash->server_ht->hmax;
						for(int j = 0; j < nr_buckets; j++) {
							ll_node_t *curr = trash->server_ht->buckets[j]->head;
							while(curr) {
								char *key, *value;
								key = ((info*)curr->data)->key;
								value = ((info*)curr->data)->value;
								if(father_server(main, key) == duplicate_id) {
									server_store(neighbour, key, value);
									curr = curr->next;
									server_remove(trash, key);
								}
								else
									curr = curr->next;
							}
						}
					}
				}
				remove_element(main, i);
			}
		duplicate_id = (k + 1) * NMAX  + server_id;
	}
	free_server_memory(trash);
}


void loader_store(load_balancer *main, char *key, char *value, int *server_id)
{
	unsigned int key_hash = hash_function_key(key);
	// we verify if the hey's hash is "greater" that the hash of the last
	// server from the hash ring

	if(key_hash >
	   hash_function_servers(&main->hash_ring[main->nr_servers - 1].server_id)) {
		// in this case it belongs to the first server
		(*server_id) = main->hash_ring[0].server_id % NMAX;
		server_store(main->hash_ring[0].server, key, value);
		return;
	}

	if(key_hash < hash_function_servers(&main->hash_ring[0].server_id)) {
		// in this case it belongs to the first server
		(*server_id) = main->hash_ring[0].server_id % NMAX;
		server_store(main->hash_ring[0].server, key, value);
		return;
	}

	// otherwise we have to search in the hash ring

	for(int i = 1; i < main->nr_servers; i++) {
		if(key_hash < hash_function_servers(&main->hash_ring[i].server_id)) {
				(*server_id) = main->hash_ring[i].server_id % NMAX;
				server_store(main->hash_ring[i].server, key, value);
				return;
			}
	}
}

char *loader_retrieve(load_balancer *main, char *key, int *server_id)
{
	unsigned int key_hash = hash_function_key(key);
	// we verify if the hey's hash is "greater" that the hash of the last
	// server from the hash ring

	if(key_hash >
	   hash_function_servers(&main->hash_ring[main->nr_servers - 1].server_id)) {
		// in this case it belongs to the first server
		(*server_id) = main->hash_ring[0].server_id % NMAX;
		char *value = server_retrieve(main->hash_ring[0].server, key);
		return value;
	}

	// otherwise we have to search in the hash ring
	char *value;
	for(int i = 0; i < main->nr_servers; i++)
		if(key_hash < hash_function_servers(&main->hash_ring[i].server_id)) {
				(*server_id) = main->hash_ring[i].server_id % NMAX;
				value = server_retrieve(main->hash_ring[i].server, key);
				break;
			}
	return value;
}

void free_load_balancer(load_balancer *main)
{
	// free each server from hash_ring
	for(int i = 0; i < main->nr_servers; i++)
		if(main->hash_ring[i].server != NULL)  {
			server_memory *trash_server = main->hash_ring[i].server;
			free_server_memory(main->hash_ring[i].server);
			main->hash_ring[i].server = NULL;
			for(int j = 0; j < main->nr_servers; j++)
				if(main->hash_ring[j].server == trash_server)
					main->hash_ring[j].server = NULL;
		}

	// free te array of hash_rings elements
	free(main->hash_ring);

	main->nr_servers = 0;  // no servers
	// free the loader
	free(main);
}
