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

/* Get current GMT datetime in HTTP format.
 * Puts result into `s`.
 */
void
getcurrdate(string s)
{
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    // xxx: return value not checked
    timeinfo = gmtime(&rawtime);
    // Assume s is blank and there is enough space.
    // Datetime should always occupy 30 characters (with '\0')
    // format is Sun, 06 Nov 1994 08:49:37 GMT
    strftime(s, MAXBUF, "%a, %d %h %Y %H:%M:%S GMT", timeinfo);
}

/* Given an empty response and a body of stuff to put in it, we
 * build out a minimal HTTP 1.1 message with headers into response.
 */
void
prepare_buf(enum status rcode, httpbuf response, httpbuf body, string mimet)
{
    // Get return string
    char *rstring;
    switch (rcode) {
    case OK:
        rstring = "HTTP/1.1 200 OK";
        break;
    case NOT_FOUND:
        rstring = "HTTP/1.1 404 Not Found";
        break;
    case SERVER_ERROR: default:
        rstring = "HTTP/1.1 500 Server Error";
        break;
    }

    // Get current date string
    string date;
    getcurrdate(date);

    if (mimet == NULL)
        mimet = "text/plain";

    // Build out headers using adjacent string concatenation
    snprintf(response, MAXBUF,
        "%s\r\n"
        "Date: %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
    rstring, date, mimet, strlen(body), body);

    return;
}


/*
 * Returns -1 if file not found (404), 0 otherwise
 */
int
build_get_body(char *fn, httpbuf body)
{
    FILE * f = fopen(fn, "r");
    if (f == NULL) {
        perror("fopen");
        return -1;
    }

    string line;
    // Get one line at a time to ensure CRLF line endings.
    // Replace the last character and the next spot in the buffer by CRLF.
    // todo: warn: this assumes a text file
    while (fgets(line, MAXBUF-1, f) != NULL) {
        size_t len = strlen(line);
        line[len-1] = '\r';
        line[len] = '\n';
        line[len+1] = '\0';
        strcat(body, line);
    }
    fclose(f);

    return 0;
}


/* Translates a basic percent-encoded string into its normal ASCII by the
 * following mapping. Assume string is valid.
 * '+               -> ' '
 * '%n' (ascii hex) -> 'n ' (ascii representation)
 */
void
translate_percent_encoding(string s)
{
    int i;
    for (i = 0; i < MAXBUF; i++) {
        char c = s[i];
        if (c == '+')
            s[i] = ' ';
        else if (c == '\0')
            break;
        else if (c == '%') {
            char *nptr = &s[i+1];
            char *endptr = &s[i+2];
            char c = strtol(nptr, &endptr, 16);
            s[i] = c;
            s[i+1] = ' ';
            s[i+2] = ' ';
            i += 2;
        }
    }

    return;
}

/* Runs a (assumed to be) safe command and puts CRLF-delimited stuff into
 * the (assumed to be empty) body.
 */
int
run_command(string cmd, httpbuf body)
{
    FILE *fp = popen(cmd, "r");
    if (fp == NULL)
        return -1;

    string path;
    while (fgets(path, MAXBUF-1, fp) != NULL) {
        size_t len = strlen(path);
        path[len-1] = '\r';
        path[len] = '\n';
        path[len+1] = '\0';
        strncat(body, path, MAXBUF-len-1);
    }

    if (pclose(fp) != 0)
        return -1;
    return 0;
}


/* Given a POST request, takes out the `command=*` query string.
 * Content-Length **must** be set.
 *
 * Returns -1 if error, 0 otherwise in which case the command value will be put
 * into the command parameter, un-percent encoded.
 */
int
get_command(httpbuf request, string command)
{
    ssize_t contentlen = -1;
    char *line = strtok(request, "\n");
    // We can safely ignore the first line since it is the HTTP status line
    while ((line = strtok(NULL, "\n")) != NULL) {
        if (strstr(line, "Content-Length") != NULL) {
            contentlen = atoi(&line[strlen("Content-Length")]);
        } else if (line[0] == '\r' || line[0] == '\0') {
            // todo: strange insistence on having content-length
            // and then not even using it for security, heh
            line = strtok(NULL, "\n");

            if (contentlen != -1) {
                char *cmd = strtok(line, "command=");
                memcpy(command, cmd, MAXBUF);
                translate_percent_encoding(command);
                return 0;
            }

            return -1;
        }
    }

    return -1;
}


int
get_mimetype(string dest, char *fn)
{
    string cmd = "file --mime-type -b ";
    // xxx: assume a safe operation
    strcat(cmd, fn);

    httpbuf mimestring;
    if (run_command(cmd, mimestring) == -1)
        return -1;

    // Get rid of two line-delimiters
    mimestring[strlen(mimestring)-2] = '\0';
    // Copy over into string
    strncpy(dest, mimestring, MAXBUF);
    dest[MAXBUF-1] = '\0';

    return 0;
}


/* Alias for run_command().
 */
int
build_post_body(string command, httpbuf body)
{
    return run_command(command, body);
}

/* Looks at first line and goes to either GET/POST which behave
 * as described in the README.
 *
 * NB: it is assumed that response is empty starting out...it is not even
 * modified in this function directly, but instead passed to other functions
 * to modify
 * NB: neither is body directly modified in this function
 */
void
process_request(httpbuf request, httpbuf response)
{
    httpbuf body = "";

    // xxx: assume HTTP/1.1 request
    if (strncasecmp(request, "GET", 3) == 0) {
        // todo: could probably use something simpler than strtok
        // xxx: modifies request
        char *s = strtok(&request[4], " ");
        if (s == NULL)
            return prepare_buf(NOT_FOUND, response, body, NULL);
        // Default to index.html when navigating to root
        // xxx: poor support for all http paths types, e.g. `./tmp/file` will
        // attempt to access fs root
        s = (strncmp(s, "/", MAXBUF) == 0) ? "index.html" : (s+1);

        string mimet = "";
        get_mimetype(mimet, s);

        if (build_get_body(s, body) == -1)
            prepare_buf(NOT_FOUND, response, body, NULL);
        else
            prepare_buf(OK, response, body, mimet);
    } else if (strncasecmp(request, "POST", 4) == 0) {
        string command;
        if (get_command(request, command) == -1)
            prepare_buf(SERVER_ERROR, response, body, NULL);
        else if (build_post_body(command, body) == -1)
            prepare_buf(SERVER_ERROR, response, body, NULL);
        else
            prepare_buf(OK, response, body, NULL);
    } else
        prepare_buf(NOT_IMPLEMENTED, response, body, NULL);

    return;
}


int
main(void)
{
    // Setup networking junk
    static struct addrinfo server;
    int serverfd = getaddrinfo_wrapper(&server);
    if (serverfd == -1)
        return EXIT_FAILURE;
    if (listen(serverfd, BACKLOG) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }

    while (true) {
        // Accept connection to server
        int clientfd;
        struct sockaddr_storage client;
        socklen_t addrlen = sizeof(struct sockaddr_storage);
        if ((clientfd = accept(serverfd, (struct sockaddr *)&client,
                &addrlen)) == -1) {
            perror("accept");
            return EXIT_FAILURE;
        }

        // Get request into buffer
        httpbuf request, response;
        if ((recv(clientfd, request, MAXBUF, 0)) == -1) {
            perror("recv");
            return EXIT_FAILURE;
        }

        // Parse, build response, set headers
        process_request(request, response);

        // Send back to originating client
        size_t len = strlen(response);
        send(clientfd, response, len, 0);
        close(clientfd);
    }

    close(serverfd);
    return EXIT_SUCCESS;
}
