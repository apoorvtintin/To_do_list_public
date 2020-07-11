/* @author Apoorv Gupta <apoorvgupta@hotmail.co.uk> */
/** @brief handles storaage of client ctx and key generation,
 * acts as an abstraction layer for the database.
 */

#include <arpa/inet.h>
#include <sys/socket.h>

#include "c_s_iface.h"
#include "server.h"

int storage_init();

void storage_deinit();

int handle_storage(client_ctx_t *client_ctx);

void export_db(char *file);

int import_db(char *file);
