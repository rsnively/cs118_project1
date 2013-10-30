/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <unistd.h>


void error(char *msg)
{
    perror(msg);
    exit(1);
}

// Retrieve the desired filename from an HTTP request message.
char* get_filename(char* request)
{   
    // Ignore the requst up to the first space.
    int pos = 0;
    while (request[pos] != ' ') {
        pos++;
    }

    // Find start of the file name (a slash).
    while (request[pos] != '/') {
        pos++;
    }
    char* start_pos = request + pos;
    int length = 0;

    // Get the file name.
    while (request[pos] != ' ') {
        pos++;
        length++;
    }
    char* filename = malloc(length * sizeof (char));
    strncpy(filename, start_pos, length);
    return filename;
}

// Get the file extension of a file.
char* getExt(char* name)
{
    char* start = strrchr(name, '.');
    char* extension = malloc(strlen(name));
    strcpy(extension, start);

    return extension;
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);	//create socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    memset((char *) &serv_addr, 0, sizeof(serv_addr));	//reset memory
    //fill in address info
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
     
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
    listen(sockfd,5);	//5 simultaneous connection at most
     
    //accept connections
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
    if (newsockfd < 0) 
        error("ERROR on accept");
         
    int n;
   	char buffer[256];
   			 
   	memset(buffer, 0, 256);	//reset memory
      
    //read client's message
    n = read(newsockfd,buffer,255);

    // Get the name of the file requested.
    char* filename = get_filename(buffer);

    // Get the current working directory.
    char filepath[256];
    getcwd(filepath, 256);
    // Add the name of the file to the current directory.
    strcat(filepath, filename);
    // Create file pointer.
    FILE *f = fopen(filepath, "r");

    // Will eventually contain our response message.
    char* response_message;
    int message_size;
    // If the file wasn't found.
    if (f == NULL)
    {
        // 404 Status
        char* response_status = "HTTP/1.1 404 Not Found\n";
 
        // 404 Body
        char* response_body = "<html><head><title>404</title></head><body><h1>404 Not Found</h1></body></html>";
        char body_length[10];
        sprintf(body_length, "%d", strlen(response_body));

        // 404 Headers
        // Content type
        char* content_type = "Content-Type: text/html\n";
        // Content length
        char* length_header = "Content-Length: ";
        char* length_end = "\n\r\n";
        char* content_length = malloc(strlen(length_header) +
                                      strlen(body_length) +
                                      strlen(length_end));
        strcpy(content_length, length_header);
        strcat(content_length, body_length);
        strcat(content_length, length_end);
        // Put it all together into header
        char* response_header = malloc (strlen(content_type) +
                                        strlen(content_length));
        strcpy(response_header, content_type);
        strcat(response_header, content_length);

        // Combine status, header, and body into message.
        message_size = strlen(response_status) + strlen(response_header) + strlen(response_body);
        response_message = malloc(message_size);
        strcpy(response_message, response_status);
        strcat(response_message, response_header);
        strcat(response_message, response_body);
    }
    else
    {
        // Check the status of the file
        struct stat file_status;
        if (stat(filepath, &file_status) == 0)
        {
            // File is a directory: Show the contents with links.
            if (file_status.st_mode & S_IFDIR)
            {
                ; // @TODO
            }
            // Regular file: Show file.
            else if (file_status.st_mode & S_IFREG)
            {
                // Response status: OK
                char* response_status = "HTTP/1.1 200 OK\n";
                // Get Response body (contents of file)
                int file_size = file_status.st_size;
                char* response_body = malloc(file_size + 1);

                char* file_extension = getExt(filename);
                
                int c = getc(f);
                int i = 0;
                while (c != EOF)
                {
                    // If the image is a jpeg, put decimal data.
                    if (strcmp(file_extension, ".jpg") == 0)
                    {
                        // @TODO doesn't fix anything...
                        response_body[i] = c;
                    }
                    else
                    {
                        response_body[i] = (char) c;
                    }
                    i++;
                    if (i > file_size)
                    {
                        error("ERROR: file too big");
                    }
                    c = getc(f);
                }
                if (strcmp(file_extension, ".jpg") != 0)
                {
                    response_body[i] = '\0';
                }

                // Response header.
                char* content_type = "Content-Type:  text/html\n";
                
                // See if file is a JPG
                if (strcmp(file_extension, ".jpg") == 0)
                {
                    // Only results in errors.
                    content_type = "Content-Type: image/jpeg\n";
                }

                char* length_header = "Content-Length: ";
                char body_length_str[10];
                sprintf(body_length_str, "%d", file_size);
                char* length_end = "\r\n\r\n";
                char* content_length = malloc(strlen(length_header) +
                                              strlen(body_length_str) +
                                              strlen(length_end));
                strcpy(content_length, length_header);
                strcat(content_length, body_length_str);
                strcat(content_length, length_end);
                
                char* response_header = malloc(strlen(content_type) +
                                               strlen(content_length));
                strcpy(response_header, content_type);
                strcat(response_header, content_length);

                // Combine status, header, and body into message.
                message_size = strlen(response_status) +
                               strlen(response_header) +
                               strlen(response_body);
                response_message = malloc(message_size);
                strcpy(response_message, response_status);
                strcat(response_message, response_header);
                strcat(response_message, response_body);
            }
            // Something else: 400 Error
            else
            {
                ; // @TODO
            }
        }
        // The file was found earlier, but wasn't found now? Something weird...
        else
        {
            error("ERROR: Shouldn't ever happen...");
        }
        
    }

    printf("\nResponse Message:\n%s\n\n", response_message);


   	if (n < 0) error("ERROR reading from socket");
   	printf("Here is the message: %s\n",buffer);
   	 
   	//reply to client

   	n = write(newsockfd,response_message,message_size);
   	if (n < 0) error("ERROR writing to socket");
         
     
    close(newsockfd);//close connection 
    close(sockfd);
         
    return 0; 
}

