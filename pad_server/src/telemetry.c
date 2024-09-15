#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "telemetry.h"

/* Helper function for returning an error code from a thread */
#define thread_return(e) pthread_exit((void *)(unsigned long)((e)))

/* Linked list of connected clients */
typedef struct {
    pthread_mutex_t lock;       /* Protect concurrent access to clients. */
    pthread_cond_t space_avail; /* Space is available for a new client */
    unsigned int connected;     /* The number of connected clients */
    int clients[MAX_TELEMETRY]; /* A list of client connections */
} client_l_t;

/* Arguments for the connection accepting thread */
struct accept_args {
    client_l_t *list; /* The list of client connections */
    int master_sock;  /* The master socket to accept connections on. */
};

/* The main telemetry socket */
typedef struct {
    int sock;
    struct sockaddr_in addr;
} telemetry_sock_t;

/*
 * Set up the telemetry socket for connection.
 * @param sock The telemetry socket to initialize.
 * @param port The port number to use to accept connections.
 * @return 0 for success, error code on failure.
 */
static int telemetry_init(telemetry_sock_t *sock, uint16_t port) {

    /* Initialize the socket connection. */
    sock->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->sock < 0) return errno;

    /* Create address */
    sock->addr.sin_family = AF_INET;
    sock->addr.sin_addr.s_addr = INADDR_ANY;
    sock->addr.sin_port = htons(port);

    /* Bind the controller socket */
    if (bind(sock->sock, (struct sockaddr *)&sock->addr, sizeof(sock->addr)) < 0) {
        return errno;
    }

    /* Listen for a sock client connection */
    if (listen(sock->sock, MAX_TELEMETRY) < 0) {
        return errno;
    }

    return 0;
}

/*
 * Closes a telemetry socket.
 * @param arg A pointer to a telemetry socket.
 * @return 0 on success, error code on error.
 */
static int telemetry_close(telemetry_sock_t *sock) {
    if (close(sock->sock) < 0) {
        return errno;
    }
    return 0;
}

/*
 * pthread cleanup handler for telemetry socket.
 * @param arg A pointer to a telemetry socket.
 * @return 0 on success, error code on error.
 */
static void telemetry_cleanup(void *arg) { telemetry_close((telemetry_sock_t *)(arg)); }

/*
 * Initializes a list of clients.
 * @param l The list to initialize.
 */
static void client_list_init(client_l_t *l) {
    l->connected = 0;
    // TODO: do I need to handle pthread errors
    pthread_mutex_init(&l->lock, NULL);
    pthread_cond_init(&l->space_avail, NULL);
    for (unsigned int i = 0; i < MAX_TELEMETRY; i++) {
        l->clients[i] = -1;
    }
}

/*
 * Add a client to the list of clients.
 * @param l The list of clients to add to.
 * @param client The client to add to the list.
 * @return 0 on success, error code on failure.
 */
static int client_list_add(client_l_t *l, int client) {

    // TODO: do I need to handle return errors on pthread functions?
    pthread_mutex_lock(&l->lock);

    /* No space, don't add client */
    while (l->connected == MAX_TELEMETRY) {
        pthread_cond_wait(&l->space_avail, &l->lock);
    }

    /* There is space, add the client */
    l->connected++;
    for (unsigned int i = 0; i < MAX_TELEMETRY; i++) {
        if (l->clients[i] == -1) {
            l->clients[i] = client;
            pthread_mutex_unlock(&l->lock);
            return 0;
        }
    }

    /* We should never get here, because this means the client list has recorded that there is space when there isn't
     * any.
     */
    assert(0 && "Client list lied about space available.");
    pthread_mutex_unlock(&l->lock);
    return EAGAIN;
}

/*
 * Remove a client from the list.
 * @param client_id The ID of the client to remove.
 * @return 0 on success, error code on failure.
 */
static int client_list_remove(client_l_t *l, int client_id) {

    assert(0 <= client_id && client_id < MAX_TELEMETRY);

    // TODO: handle pthread function errors?
    pthread_mutex_lock(&l->lock);
    l->clients[client_id] = -1;
    pthread_mutex_unlock(&l->lock);

    return 0;
}

/*
 * Thread to continuously accept new connections as long as there's space.
 * @param arg Arguments to the connection accepting thread of type `struct accept_args`.
 * @return The thread's exit code.
 */
static void *telemetry_accept_thread(void *arg) {

    struct accept_args *args = (struct accept_args *)(arg);
    int new_client;
    int err;

    for (;;) {

        /* Accept the first incoming connection. */
        new_client = accept(args->master_sock, NULL, 0);
        if (new_client < 0) {
            fprintf(stderr, "Couldn't accept connection with error: %s\n", strerror(errno));
        }

        /* Add the client to the list. */
        err = client_list_add(args->list, new_client);
        if (err) {
            fprintf(stderr, "Couldn't add new client to the list with error: %s\n", strerror(errno));
        }
        printf("New telemetry client connected\n");
    }

    thread_return(0);
}

/*
 * Cleanup function to kill a thread.
 * @param arg A pointer to the pthread_t thread handle.
 */
static void cancel_wrapper(void *arg) { pthread_cancel(*(pthread_t *)(arg)); }

/*
 * Run the thread responsible for transmitting telemetry data.
 * @param arg The arguments to the telemetry thread, of type `telemetry_args_t`.
 */
void *telemetry_run(void *arg) {

    telemetry_args_t *args = (telemetry_args_t *)(arg);
    char buffer[BUFSIZ];
    int err;

    /* Null telemetry file means nothing to do */
    if (args->data_file == NULL) {
        printf("No telemetry data to send.\n");
        thread_return(0);
    }

    /* Start telemetry socket */
    telemetry_sock_t telem;
    err = telemetry_init(&telem, args->port);
    if (err) {
        fprintf(stderr, "Could not start telemetry socket: %s\n", strerror(err));
        thread_return(err);
    }
    pthread_cleanup_push(telemetry_cleanup, &telem);

    /* Create client list */
    client_l_t list;
    client_list_init(&list);

    /* Start thread to accept new connections */
    pthread_t accept_thread;
    struct accept_args accept_thread_args = {.list = &list, .master_sock = telem.sock};
    err = pthread_create(&accept_thread, NULL, telemetry_accept_thread, &accept_thread_args);
    if (err) {
        fprintf(stderr, "Could not create thread to accept new connections: %s\n", strerror(err));
        thread_return(err);
    }
    pthread_cleanup_push(cancel_wrapper, &accept_thread);

    /* Open telemetry file */
    FILE *data = fopen(args->data_file, "r");
    if (data == NULL) {
        fprintf(stderr, "Could not open telemetry file \"%s\" with error: %s\n", args->data_file, strerror(errno));
        thread_return(err);
    }

    /* Start transmitting telemetry to active clients */
    for (;;) {

        /* Handle file errors */
        if (ferror(data)) {
            fprintf(stderr, "Error reading telemetry file: %s\n", strerror(errno));
            thread_return(err);
        }

        /* Read from file in a loop */
        if (feof(data)) rewind(data);

        /* Read next line */
        if (fgets(buffer, sizeof(buffer), data) == NULL) {
            continue;
        }

        /* TODO: parse data */
        header_p hdr = {.type = TYPE_TELEM, .subtype = TELEM_PRESSURE};
        pressure_p pkt = {.id = 0, .time = 0, .pressure = 1000};

        /* Send data to all clients. */
        ssize_t sent;

        // TODO: handle errors from mutex function calls
        // TODO: client list could have some API for sending to all?
        // How to handle needing multiple synchronized sends behind monitor? (one for header, one for packet)
        err = pthread_mutex_lock(&list.lock); // Exclusive access to clients

        for (unsigned int i = 0; i < MAX_TELEMETRY; i++) {
            if (list.clients[i] == -1) continue; // Client is not connected

            sent = send(list.clients[i], &hdr, sizeof(hdr), 0);

            if (sent == 0) {
                printf("Client %u disconnected.\n", i);
                client_list_remove(&list, i);
                continue;
            }

            if (sent == -1) {
                fprintf(stderr, "Could not send to client %u with error: %s\n", i, strerror(errno));
                client_list_remove(&list, i);
                continue;
            }

            err = send(list.clients[i], &pkt, sizeof(pkt), 0);
            if (sent == 0) {
                printf("Client %u disconnected.\n", i);
                client_list_remove(&list, i);
                continue;
            }

            if (sent == -1) {
                fprintf(stderr, "Could not send to client %u with error: %s\n", i, strerror(errno));
                client_list_remove(&list, i);
                continue;
            }
        }

        err = pthread_mutex_unlock(&list.lock); // Release access to client list
    }

    thread_return(0); // Normal return

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
}
