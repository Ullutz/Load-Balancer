/* Copyright 2023 <> */
#include <stdlib.h>
#include <string.h>

#include "load_balancer.h"
#include "list.h"

#define ticket 100000

struct hashring_t {
    int hash;
    int label;
};

struct load_balancer {
    server_memory **servers;
    int size;
    hashring_t **hashring;
    int hashrn_size;
    int max_size;
};

unsigned int hash_function_servers(void *a) {
    unsigned int uint_a = *((unsigned int *)a);

    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = (uint_a >> 16u) ^ uint_a;
    return uint_a;
}

unsigned int hash_function_key(void *a) {
    unsigned char *puchar_a = (unsigned char *)a;
    unsigned int hash = 5381;
    int c;

    while ((c = *puchar_a++))
        hash = ((hash << 5u) + hash) + c;

    return hash;
}

server_memory *find_server(load_balancer *main, int et) {
    for (int i = 0; i < main->size; i++) {
        int h = main->servers[i]->id;
        if (h == et)
            return main->servers[i];
    }
    return NULL;
}

/*function to insert the new server into the hashring*/
void insert(hashring_t **vec, hashring_t *elem, int pos, int size) {
    for (int i = size - 1; i > pos; i--)
        vec[i] = vec[i - 1];
    vec[pos] = elem;
}

void relocate_objects(load_balancer *main, int et, int pos) {
    server_memory *server_in = find_server(main, et);
    server_memory *server_out = find_server(main, main->hashring[pos]->label % ticket);
    int hash = main->hashring[pos]->hash;
    
    for (int i = 0; i < server_out->ht->size; i++) {
        ll_node_t *curr = server_out->ht->buckets[i]->head;
        char *key = (char *)(((info *)curr->data)->key);
        char *value = (char *)(((info *)curr->data)->value);

        if ((int) hash_function_key(key) < hash) {
            server_remove(server_out, key);
            server_store(server_in, key, value);
        }
        curr = curr->next;
    }
}

load_balancer *init_load_balancer() {
    /* TODO 1 */
    ///first i allocated the main structure
    load_balancer *main = (load_balancer *) malloc(sizeof(load_balancer));
    DIE(!main, "Alloc failed");

    ///i initialize the number of servers and the number of elements 
    ///on the hashring
    main->max_size = 10;
    main->size = 0;
    main->hashrn_size = 0;

    ///this is the array of servers that i ll do operations with
    main->servers = malloc(10 * sizeof(server_memory *));
    DIE(!main->servers, "Alloc failed");

    ///this is the main hashring that ll help me do the add and remove server
    ///operations
    main->hashring = malloc(30 * sizeof(hashring_t *));
    DIE(!main->hashring, "Alloc failed");
    return main;
    ///return NULL;
}

void loader_add_server(load_balancer *main, int server_id) {
    /* TODO 2 */

    ///putting the server on the server array
    int hash = hash_function_servers(&server_id);
    server_memory *server = init_server_memory();
    server->id = server_id;
    if (main->size == 1) {
    	main->servers[0] = server;
    	main->size++;
    } else {
	    for (int i = 0; i < main->size; i++) {
		int h = main->servers[i]->id;
		h = hash_function_servers(&h);
		if (hash < h) {
		    for (int j = main->size - 1; j > i; j--)
		        main->servers[j] = main->servers[j - 1];
		    main->servers[i] = server;
		    ///incresing the size of the servers array and reallocating
    			///the array if necesary
		    main->size++;
		    if (main->size == main->max_size) {
			main->max_size *= 2;
			main->servers = realloc(main->servers,
				                main->max_size * sizeof(server_memory *));
			DIE(!main->servers, "Realloc failed");
			main->hashring = realloc(main->hashring,
				                 main->max_size * 3 * sizeof(hashring_t *));
			DIE(!main->hashring, "Realloc failed");
		    }
		    break;
		}
	     }
    }

    ///putting the replicas of the servers on the hashring
    int copy = 0;
    for (int c = 0; c < 3; c++) {
        ///for each replica i calculate the label
        int et = copy * ticket + server_id;
        int hash = hash_function_servers(&et);
        ///creating a new element for the hashring
        hashring_t *elem = malloc(sizeof(hashring_t));
        DIE(!elem, "alloc failed");
        elem->hash = hash;
        elem->label = et;
        
        if (main->hashrn_size == 0) {
            insert(main->hashring, elem, 0, main->hashrn_size);
            main->hashrn_size++;
        } else {
       	    int i;
            for (i = 0; i < main->hashrn_size; i++) {
                if (main->hashring[i]->hash > hash) {
                    insert(main->hashring, elem, i, main->hashrn_size);
                    ///when i find the first slot that can be used by the replica
                    ///i stop the search, relocate the elements that need to be
                    ///relocated and going to the next replica
                    relocate_objects(main, et % ticket, i);
                    break;
                } else if (main->hashring[i]->hash == hash) {
                    ///this case, if the hashes of the two servers are equals,
                    ///checks which server has the bigget id
                    if (main->hashring[i]->label % ticket > server_id) {
                        insert(main->hashring, elem, i, main->hashrn_size);
                        relocate_objects(main, et % ticket, i);
                    } else insert(main->hashring, elem, i + 1, main->hashrn_size);
                    break;
                }
            }
            main->hashring[i] = elem;
            main->hashrn_size++;
        }
        copy++;
    }
}

void loader_remove_server(load_balancer *main, int server_id) {
    
    ///going throuh the hashring to see where i should relocate
    ///elements
    for (int i = 0; i < main->hashrn_size; i++) {
        ///seeing if the this entry on the hashring is the replica of
        ///the server i want to remove
        if (main->hashring[i]->label % ticket == server_id) {
            ///if so i get the pointers of the server i remove and the server
            ///i ll maybe relocate the elements if necesey
            server_memory *server_in = find_server(main, main->hashring[i]->label % ticket);
            server_memory *server_out = find_server(main, main->hashring[i + 1]->label % ticket);
            int hash_copy = hash_function_servers(&(server_out->id));

            ///here i go through the server elements
            for (int j = 0; j < server_in->ht->size; j++) {
                ll_node_t *curr = server_in->ht->buckets[i]->head;
                //i get the key and the value of the element i need to relocate
                char *key = (char *)(((info *)curr->data)->key);
                char *value = (char *)(((info *)curr->data)->value);

                ///honestly if i think about it this verification wasnt
                ///necesary because the hashring makes sure that every object
                ///that was on this replica of the server will 100% move to the
                ///next entry on the hashring but i wanted to be sure:)
                if ((int)hash_function_key(key) < hash_copy) {
                    server_remove(server_in, key);
                    server_store(server_out, key, value);
                }
                curr = curr->next;
            }

            ///removing the entry of the replica from the hashring
            for (int j = i; j < main->hashrn_size - 1; j++)
                main->hashring[j] = main->hashring[j + 1];
        }
    }

    ///dealloc the memory used by the server
    server_memory *server = find_server(main, server_id);
    free_server_memory(server);

    ///removing it from the array of servers
    for (int i = 0; i < main->size; i++) {
        if (main->servers[i]->id == server_id) {
            for (int j = i; j < main->hashrn_size - 1; j++)
                main->servers[j] = main->servers[j + 1];
            break;
        }
    }

    ///reseting the size paramters for the array and the hashring
    main->size--;
    main->hashring -= 3;
} 

void loader_store(load_balancer *main, char *key, char *value, int *server_id) {
    /* TODO 4 */
    ///first i need to go through the hashring to see which replica will
    ///contain the new element
    int label;
    for (int i = 0; i < main->hashrn_size; i++) {
        int hash = hash_function_key(key);
        if  (hash < main->hashring[i]->hash)
            label = main->hashring[i]->label;
    }

    label = label % ticket;
    ///finding the server which should contain the element
    server_memory *server = find_server(main, label);
    ///putting it there
    server_store(server, key, value);
    ///returining the id via the server_id parameter
    *server_id = label;
}

char *loader_retrieve(load_balancer *main, char *key, int *server_id) {
    /* TODO 5 */
    ///first i need to go through the hashring to see which replica
    ///should contain the element
    int label;
    for (int i = 0; i < main->hashrn_size; i++) {
        int hash = hash_function_key(key);
        if  (hash < main->hashring[i]->hash)
            label = main->hashring[i]->label;
    }

    label = label % ticket;

    ///finding the server which should contain the element
    server_memory *server = find_server(main, label);
    ///this function return NULL if the element if the key is not found
    ///so i can just return the pointer.
    *server_id = label;
    char *value = server_retrieve(server, key);

    return value;
}

void free_load_balancer(load_balancer *main) {
    /* TODO 6 */
    ///first i need to free the hashring and the array of servers
    for (int i = 0; i < main->hashrn_size; i++) {
        free(main->hashring[i]);
    }
    free(main->hashring);

    for (int i = 0; i < main->size; i++) {
        free(main->servers[i]);
    }
    free(main->servers);

    ///then i free the load itself
    free(main);
}
