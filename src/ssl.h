#ifndef SSL_H
#define SSL_H

#include <s2n.h>
#include "net.h"

struct s2n_config *ssl_init();
void ssl_cleanup();

status ssl_connect(connection *, char *);
status ssl_close(connection *);
status ssl_read(connection *, size_t *);
status ssl_write(connection *, char *, size_t, size_t *);
size_t ssl_readable(connection *);

#endif /* SSL_H */
