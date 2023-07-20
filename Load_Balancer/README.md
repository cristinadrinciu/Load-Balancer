Load Balancer

the program simulates a load balancer, based on consistent hashing, implemented with a hashring.

The program is divided in multiple files: loader.c, server.c and urils.c.
In utils.h are all the functions and structs needed for the hashtable and linked lists and in utilis.c are their implementaintion.

In server.h and in server.c are the implementations for the functions needed for servers. I declared the struct server_memory as a structure that has just a pointer to an hashtable, because a server itself is an hashtable with objects.
So in server.c each function consists in calling the associated function for the hashtable, plus where is needed, allocating the structer server_memory itself (in init_server_memory) and freeing it (in free_server_memory).

In loader.h and loader.c are the functions needed for a load balancer.
I declared the structure "hash_ring_elem" to contain the server_id, which could be also the server_id for the duplicates. So, for example, it could be 1, 2, 3, etc but also 100001, 100002, 100003 or 200001, 200002, 200003.
Next it has a pointer to a server. For each server and its duplicates, they point to the same server.

I also declared a struct "load_balancer" that containts a resizable array of hash_ring_elem structures and also and integer that contains the number of total servers in the hash ring (including the dublicates);

In init_load_balancer I allocated memory for the loader and for the hash ring.

The function add_element adds an element in the array, inserting an hash ring element given as a parameter.

The function father_server receives a key as a parametre and it returns the server_id of the server it belongs, which also may be the id of a duplicate.

Function Redistribution redistributes the objects from the right neighbour when a new server is added in the hash ring. Using the function father_server we identify which objects from the server belong to the specific hash ring element with the server id.

Because the duplicates also point to the same server, when we want to check the objects from the server we have to make sure that we check and manipulate the objects that belong to the dublicate we work with.

In function loader_add_server we have two cases: when we add the element in the hashring for the first time or therwise. First the server and its duplicates are introduced in order in the array, so that it is sorted. After that we redistribute the objects from the right neighbour server.

Function remove_element removes an element from the array and resises it.

In function loader_remove_server I declare a server_memory variable called trash that will point to the to be deleted server. For each duplicates the objects are redistributed to the right neighbour, also choosing the correct ones by calling the function father_server. After that, the objects are removed from the to be deleted server. In the end, the serrver is freed by calling free_server_memory function.

In function loader_store I find the first server that has a "greater" hash than the given key and I store it there by calling server_store function. The server_id that is returned in this function is the one for the original one, not from the duplicates.

In functionloader_retrieve is the same aproach as in loader_store, the only differene is that when I find the right server the key belongs to, I return the value by using the function server_retrive.

In function free_load_balancer I free al the resources. I free each server from the hashring, being careful so that when I free the first server from a set of duplicates, I set the server for the rest NULL, so that I do not have a double free. I free the array and then I free the structure for the load_balancer.

In main is coded the calling for each operation, structured in a clean way.
