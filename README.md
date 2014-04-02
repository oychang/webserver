An HTTP/1.1 server implementating GET and POST.

Spring 2014, University of Miami, CSC524: Computer Networks

    Project 3: Webserver Project
    Due: Monday, 7 April

    Information on the HTTP Protocol.
    Implement GET and POST.
    For GET queries you will need to understand percent encoding.
    First implement a server of fixed pages, then launch processes to handle the request, if it is a POST or a GET with a query appended.
    To serve a fixed page, the URL will give the name of a file (if it is a pathname, send back a not found error), and send that back to the client, with the appropriate headers added: Content-Type and Content-Length are really necessary.
    If the URL is a query or a POST, start the program named in the URL, send the query to the program through stdin and stream to the requester what the program writes to stdout.
    To direct your browser to test your server, you can use the syntax ec2-50-19-171-60.compute-1.amazonaws.com:3337 for port 3337.
    See information on piping stdin and stdout for more help with this.


# Steps

1. socket()/bind()/listen() for TCP packets coming to PORT
2. fork() & accept connection
3. Read information with recv() into a fixed size buffer
4. Parse headers of client request
5. Attempt to construct a response
6. Send response
7. Die
