// Copyright (C) 2013 - Will Glozer.  All rights reserved.
// s2n-tls port for wrk3.

#include <s2n.h>
#include "ssl.h"

struct s2n_config *ssl_init() {
    struct s2n_config *cfg = NULL;

    if (s2n_init() != S2N_SUCCESS) return NULL;

    cfg = s2n_config_new();
    if (!cfg) return NULL;

    s2n_config_disable_x509_verification(cfg);

    return cfg;
}

void ssl_cleanup() {
    s2n_cleanup();
}

status ssl_connect(connection *c, char *host) {
    s2n_blocked_status blocked;

    s2n_connection_set_config(c->s2n_conn, c->s2n_cfg);
    s2n_connection_set_fd(c->s2n_conn, c->fd);
    s2n_set_server_name(c->s2n_conn, host);
    s2n_connection_set_blinding(c->s2n_conn, S2N_SELF_SERVICE_BLINDING);

    if (s2n_negotiate(c->s2n_conn, &blocked) != S2N_SUCCESS) {
        if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
            return RETRY;
        }
        return ERROR;
    }
    return OK;
}

status ssl_close(connection *c) {
    s2n_blocked_status blocked;
    s2n_shutdown(c->s2n_conn, &blocked);
    s2n_connection_wipe(c->s2n_conn);
    return OK;
}

status ssl_read(connection *c, size_t *n) {
    s2n_blocked_status blocked;
    ssize_t r = s2n_recv(c->s2n_conn, c->buf, sizeof(c->buf), &blocked);
    if (r < 0) {
        if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
            return RETRY;
        }
        return ERROR;
    }
    if (r == 0) return READ_EOF;
    *n = (size_t) r;
    return OK;
}

status ssl_write(connection *c, char *buf, size_t len, size_t *n) {
    s2n_blocked_status blocked;
    ssize_t r = s2n_send(c->s2n_conn, buf, len, &blocked);
    if (r < 0) {
        if (s2n_error_get_type(s2n_errno) == S2N_ERR_T_BLOCKED) {
            return RETRY;
        }
        return ERROR;
    }
    *n = (size_t) r;
    return OK;
}

size_t ssl_readable(connection *c) {
    return (size_t) s2n_peek(c->s2n_conn);
}
