/* @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */
/** @brief handles storaage of client ctx and key generation,
 * acts as an abstraction layer for the database.
 */

#include "server.h"

int storage_init();

void storage_deinit();

//int generate_key(client_ctx_t *client_ctx);

int handle_storage(client_ctx_t *client_ctx);