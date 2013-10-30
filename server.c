/**
 * Zach North - 603 885 768
 * Rory Snively - 803 889 336
 * 
 * CS 118 - Project 1
 * UCLA, Fall 2013
 *
 *
 * This code runs a simple server in the internet domain using TCP. The port
 * number is passd as an argument. Once a port is established, the server runs
 * until shut down. Every time a new connection is made, the server forks a new
 * process to handle it.
 */

#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

// Size of the buffer used to read request message header.
const int REQUEST_BUFFER_SIZE = 1024;

// Prints an error message, and then exits.
void error(char* msg)
{
    perror(msg);
    exit(1);
}

// Handler for connection processes.
void connection_handler(int signal)
{
    // Wait for connection to terminate.
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
        ;
    }
}

// Retrieves the name of the file desired by an HTTP request.
char* get_filename(char* request)
{
    // Ignore the request up to the first space.
    int pos = 0;
    while (request[pos] != ' ')
    {
        pos++;
    }

    // Find the start of the file name (a slash).
    while (request[pos] != '/')
    {
        pos++;
    }

    char* start_pos = request + pos;
    int length = 0;

    // Get the file name.
    while (request[pos] != ' ')
    {
        pos++;
        length++;
    }
    char* filename = malloc(length * sizeof(char));
    strncpy(filename, start_pos, length);
    return filename;
}

// Gets the extension of a file.
char* get_file_extension(char* filename)
{
    // Find last '.' character.
    char* start = strrchr(filename, '.');
    // Longer than necessary, but oh well.
    char* extension = malloc(strlen(filename));

    // Copy everything after '.' into extension.
    strcpy(extension, start);
    return extension;
}

// Returns the proper Content-Type header line for an HTTP response given
// the file extension of the requested file.
char* get_content_type(char* ext)
{
    char* type = "Content-Type: text/plain\r\n";
    
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
    {
        type = "Content-Type: text/html\r\n";
    }
    else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
    {
        type = "Content-Type: image/jpeg\r\n";
    }
    else if (strcmp(ext, ".gif") == 0)
    {
        type = "Content-Type: image/gif\r\n";
    }

    return type;
}

// Creates the response message for a 404 error.
char* create_404_response()
{
    // Generate required headers and such.
    char* status = "HTTP/1.1 404 Not Found\r\n";
    char* body = "<html><head><title>404</title></head><body><h1>404 Not Found</h1></body></html>";
    char* length_1 = "Content-Length: ";
    char length_2[10];
    sprintf(length_2, "%d", strlen(body));
    char* length_3 = "\r\n";
    char* type = "Content-Type: text/html\r\n\r\n";

    // Allocate space for total response.
    char* response = malloc(strlen(status) +
                            strlen(length_1) +
                            strlen(length_2) +
                            strlen(length_3) +
                            strlen(type) +
                            strlen(body));

    // Copy status, headers, and body into response.
    strcpy(response, status);
    strcat(response, length_1);
    strcat(response, length_2);
    strcat(response, length_3);
    strcat(response, type);
    strcat(response, body);

    return response;
}

// Responds to an HTTP request the appropriate HTTP response.
void handle_http_request(int sock)
{
    // Read the request header.
    int n;
    char request_buffer[REQUEST_BUFFER_SIZE];
    bzero(request_buffer, REQUEST_BUFFER_SIZE);
    n = read(sock, request_buffer, REQUEST_BUFFER_SIZE - 1);
    if (n < 0)
    {
        error("ERROR reading from socket");
    }

    // Print the request message to the console.
    printf("HTTP Request Message:\n%s\n", request_buffer);

    /** Generate the response message **/
    
    char* response_msg;

    // Get the desired file.
    char* filename = get_filename(request_buffer);
    char filepath[256];
    getcwd(filepath, 256);
    strcat(filepath, filename);
    FILE *f = fopen(filepath, "r");
    
    // File not found. Return 404 message.
    if (f == NULL)
    {
        response_msg = create_404_response();
        n = write(sock, response_msg, strlen(response_msg));
        if (n < 0)
        {
            error("ERROR writing to socket");
        }
    }
    // Generate response message containing file contents.
    else
    {
        // Get the size of the file contents.
        fseek(f, 0, SEEK_END);          // Find end of file.
        size_t file_size = ftell(f);    // Get size.
        fseek(f, 0, SEEK_SET);          // Return to beginning of file.

        // Read contents of file.
        char* file_contents = malloc(file_size * sizeof(char));
        size_t amount_read = fread(file_contents, 1, file_size, f);
        
        // Determine the type of data being delivered.
        char* file_extension = get_file_extension(filename);
        char* content_type = get_content_type(file_extension);

        // Generate the response header.
        asprintf(&response_msg, "HTTP/1.1 200 OK\r\n%sContent-Length: %zi\r\n\r\n", content_type, file_size + 2);
        // Write the status and header.
        n = write(sock, response_msg, strlen(response_msg));
        if (n < 0)
        {
            error("ERROR writing response header");
        }
        // Write the contents.
        n = write(sock, file_contents, amount_read);
        if (n < 0)
        {
            error("ERROR writing file contents");
        }
    }
}

// Establishes the given socket as an access point to our server. Then spawns
// off processes for each connection that is made to serve the proper response.
int main(int argc, char* argv[])
{
    // Initialize connection to port using the given socket.
    int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    struct sigaction sig_action;

    // Make sure the arguments are correctly supplied.
    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    // Connect to a socket.
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("ERROR opening socket");
    }
    
    // Establish correspondence between socket and supplied port number.
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        error("ERROR on binding");
    }

    // Only listen on up to 5 processes at a time.
    listen(sockfd, 5);

    clilen = sizeof(cli_addr);

    // Reap all of the zombie processes that have finished their request and
    // received their response.
    sig_action.sa_handler = connection_handler;
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sig_action, NULL) == -1)
    {
        error("ERROR on signal action");
    }

    // Receive incoming connections, and fork new processes for them.
    while (1)
    {
        newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        if (newsockfd < 0)
        {
            error("ERROR on accept");
        }

        pid = fork();
        if (pid < 0)
        {
            error("ERROR on fork");
        }

        // The child process generates the HTTP response for its request and
        // then exits.
        if (pid == 0)
        {
            close(sockfd);
            handle_http_request(newsockfd);
            exit(0);
        }
        // The parent does not need the new socket, so closes it.
        else
        {
            close(newsockfd);
        }
    }

    return 0;
}
