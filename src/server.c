#include "server.h"

/* Get a valid socket to listen use with PORT.
 * Returns -1 if failed to get a socket, otherwise a valid
 * bound socket file descriptor.
 *
 * References:
 * http://www.beej.us/guide/bgnet/output/html/multipage/getaddrinfoman.html
 * http://www.beej.us/guide/bgnet/output/html/multipage/acceptman.html
 */
int
getaddrinfo_wrapper(struct addrinfo *p)
{
    struct addrinfo hints, *results;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;

    int ai_error = getaddrinfo(NULL, PORT, &hints, &results);
    if (ai_error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ai_error));
        return -1;
    }

    // getaddrinfo() returns a linked list of results
    int sockfd;
    for (p = results; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            continue;
        } else if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        } else
            break;
    }
    freeaddrinfo(results);

    if (p == NULL)
        return -1;
    return sockfd;
}




// XXX: unsafe strcat
void
prepare_buf(buf body, enum status s)
{
    buf b = {};
    // HTTP/1.1 200 OK
    switch (s) {
    case OK:
        strcat(b, "HTTP/1.1 200 OK\r\n");
        break;
    case NOT_FOUND:
        strcat(b, "HTTP/1.1 404 Not Found\r\n");
        break;
    case SERVER_ERROR:
        strcat(b, "HTTP/1.1 500 Server Error\r\n");
        break;
    }

    // according to rfc, we can return asctime(), but not good
    // Date: Fri, 31 Dec 1999 23:59:59 GMT
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = gmtime(&rawtime);
    sprintf(b, "%s\r\n", asctime(timeinfo));

    // xxx: only mime type available
    // Content-Type: text/plain
    strcat(b, "Content-Type: text/plain\r\n");

    // Content-Length: 42
    sprintf(b, "Content-Length: %zu\r\n", strlen(body));

    // Newline
    strcat(b, "\r\n");

    // Body
    if (strlen(body) > 0)
        strcat(b, body);

    return;
}


// GET / HTTP/1.1
// Host: localhost:3421
// Connection: keep-alive
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8
// User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Ubuntu Chromium/33.0.1750.152 Chrome/33.0.1750.152 Safari/537.36
// Accept-Encoding: gzip,deflate,sdch
// Accept-Language: en-US,en;q=0.8
void
process(buf b, buf send)
{
    char * s = strtok(b, " ");
    if (strncasecmp(s, "GET", 3) == 0) {
        s = strtok(NULL, " ");
        // default to index.html
        s = (strncmp(s, "/", MAXBUF) == 0) ? "index.html" : (s+1);
        FILE * f = fopen(s, "r");
        if (f == NULL) {
            perror("fopen");
            prepare_buf(send, NOT_FOUND);
            return;
        }

        string line;
        while (fgets(line, MAXBUF-1, f) != NULL) {
            size_t len = strlen(line);
            // Replace the last character and the next
            // spot in the buffer by CRLF.
            // Guaranteed to be accessible because
            // of fgets.
            line[len-1] = '\r';
            line[len] = '\n';
            line[len+1] = '\0';
            strcat(send, line);
        }

        fclose(f);

        prepare_buf(send, OK);
        return;
    } else if (strncasecmp(s, "POST", 4) == 0) {
        printf("%s\n", b);
    }

    prepare_buf(send, SERVER_ERROR);
    return;
}


int
send_reponse(buf sendbuf, int sockfd)
{
    size_t len = strlen(sendbuf);
    ssize_t sentbytes = send(sockfd, sendbuf, len, 0);
    printf("tried to send %zu...send %zd\n", len, sentbytes);
    return 0;
}


int
main(void)
{
    static struct addrinfo server;
    int sockfd = getaddrinfo_wrapper(&server);
    if (sockfd == EXIT_FAILURE)
        return EXIT_FAILURE;
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }

    int clientfd;
    struct sockaddr_storage client;
    socklen_t addrlen = sizeof(struct sockaddr_storage);
    if ((clientfd = accept(sockfd, (struct sockaddr *)&client, &addrlen)) == -1) {
        perror("accept");
        return EXIT_FAILURE;
    }

    // http://www.jmarshall.com/easy/http/
    buf request, response;
    if ((recv(clientfd, request, MAXBUF, 0)) == -1) {
        perror("recv");
        return EXIT_FAILURE;
    }

    process(request, response);
    send_reponse(response, clientfd);

    return EXIT_SUCCESS;
}