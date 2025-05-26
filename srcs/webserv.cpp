#include "webserv.hpp"

/*
    BRIEF : 
    ✅ Common Socket Syscalls (for a TCP server)

    1️⃣ Create socket	socket()	Creates a new socket file descriptor (FD).
    1. -int fd = socket(AF_INET, SOCK_STREAM, 0);
       -creates a socket using IPv4 (AF_INET) and TCP (SOCK_STREAM).

    2️⃣ Bind address	bind()	Assigns an address (IP + port) to socket.
    2. -bind(fd, (struct sockaddr*)&addr, sizeof(addr));
       -binds socket to an IP and port so  kernel knows where to listen.
    
    3️⃣ Listen for clients	listen()	Marks socket as passive (ready to accept connections).
    3.  -listen(fd, backlog);
        -enables socket to accept incoming connections (backlog = queue size).

    4️⃣ Accept connection	accept()	Accepts an incoming connection, returns a new socket FD for that client.
    4.  -int client_fd = accept(server_fd, NULL, NULL);.
        -accepts client connection. returns new FD for communicating with that client.


    5️⃣ recv() / read()	read data/bytes from client socket.
    5. -recv(client_fd, buffer, sizeof(buffer), 0);
        // or
       -read(client_fd, buffer, sizeof(buffer));

    6️⃣ send() / write()sends data back to client socket.
    6. - send(client_fd, response, length, 0);
        // or
        write(client_fd, response, length);

    7️⃣ close socket	close()	closes socket file descriptor.
    7. - close(fd); 
       - closes the socket (server or client).
*/


Webserv::Webserv(void)
{
    std:: cout << "WEBSERVER CAME !"<< std::endl;
}

Webserv::~Webserv()
{
    for(uint32_t i = 0; i < this->servers.size(); i ++)
    {
        ServerConfig serv_ = this->servers[i];
        if (serv_.client_socket > 0) {
            close(serv_.client_socket);
            std::cout << "Client socket closed." << std::endl;
        }

        if (serv_.server_socket > 0) {
            close(serv_.server_socket);
            std::cout << "Server socket closed." << std::endl;
        }
    }
}


void Webserv::init(void)
{
    for(uint32_t i = 0; i < this->servers.size(); i ++)
    {
        ServerConfig &serv_ = this->servers[i];
        if(serv_.host.empty() || serv_.port <= 0)
        {
            std::cerr << "Wrong .conf initialization [ports | domain | methods | root]!" << std::endl;
            continue;
        }

        serv_.client_addr_len = sizeof(serv_.client_addr);
        // create server socket
        serv_.server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (serv_.port < 1024)
            std::cerr << "Warning: Using a port below 1024 may require elevated privileges." << std::endl;
        if (serv_.server_socket < 0)
        {
            perror("Error creating socket");
            continue;
        }else {
            // std::cout << "Socket created successfully, server_socket = " << serv_.server_socket  << std::endl;
            // Allow address reuse
            int opt = 1;
            if (setsockopt(serv_.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
                std::cerr << "Error setting socket options: " << strerror(errno) << std::endl;
                close(serv_.server_socket);
                continue;;
            }
        }
     
        memset(&serv_.server_addr, 0, sizeof(serv_.server_addr));
        serv_.server_addr.sin_family = AF_INET;
        serv_.server_addr.sin_port = htons(serv_.port);
        // host string to network address (binary form)
        in_addr_t address = inet_addr(serv_.host.c_str());
        serv_.server_addr.sin_addr.s_addr = address; // like "127.0.0.1"
        // check if ip address is valid
        if (address == INADDR_NONE)
        {
            std::cerr << "Invalid IP address format: " << serv_.host << std::endl;
            close(serv_.server_socket);
            exit(EXIT_FAILURE);
        }

  
        // bind socket to address and port
        // (turns [address, port] -> [fd] )
        if (bind(serv_.server_socket, (struct sockaddr*)&serv_.server_addr, sizeof(serv_.server_addr)) < 0)
        {
            if (errno == EADDRINUSE){
                std::cerr << "\033[36mServer["<<i << "] port "<<  serv_.host << ":" << serv_.port << " is already in use! \033[0m" << std::endl;
                close(serv_.server_socket);
                continue;
            }
            std::cerr << "Error binding socket: " << strerror(errno) << std::endl;
            close(serv_.server_socket);
            continue;
        }

        // Listen for incoming connections
        if (listen(serv_.server_socket, SOMAXCONN) < 0)
        {
            std::cerr << "Error listening on socket: " << strerror(errno) << std::endl;
            close(serv_.server_socket);
            return;
        }
        std::cout << "\033[32mServer[" << i << "] is ready on  " << serv_.host << ":" << serv_.port  << "\033[0m "<< std::endl;
    }
}
static std::string resolve_path(const ServerConfig& server, const std::string& uri)
{
    // find matching location path
    const LocationConfig* loc = NULL;
    for (unsigned long   i = 0; i < server.locations.size(); i++)
    {    
        const LocationConfig l = server.locations[i];
        if (uri.find(l.path) == 0) {
            loc = &l;
            break;
        }
    }
    if (!loc) return ""; // no mtching location

    // URI -> full path
    std::string 
        full_path = loc->root + (uri.substr(loc->path.length()));

    struct stat st;
    if (stat(full_path.c_str(), &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
        {
            // if directory, check index files
            for(unsigned long i = 0; i < loc->index_files.size(); i ++)
            {
                const std::string& index = loc->index_files[i];
                std::string index_path = full_path + "/" + index;
                if (access(index_path.c_str(), R_OK) == 0)
                    return index_path; // Found valid index file
            }

            // if autoindex enabled, return directory marker or empty
            if (loc->autoindex)
                return "[AUTOINDEX]"; // special handling elsewhere
            else
                return ""; // no autoindex => 404
        }else
            return full_path; // regular file
    }
    return ""; // file doesn't exist
}

// handle a client request (send a basic HTTP response)
void Webserv::handle_client(int client_socket, const ServerConfig &serv)
{
    char buffer[1024];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0)
    {
        std::cerr << "Error receiving data from client!  : " << client_socket << std::endl;
        close(client_socket);
        return;
    }
    buffer[bytes_received] = '\0'; // null-terminate received data
    // log  received HTTP request
    std::cout << "Received request:\n" << buffer << std::endl;
 
    std::string uri = "/"; // Assume you parsed this from the HTTP request
    std::string path = resolve_path(serv, uri);

    if (path.empty()) 
    {
        // Construct a basic HTTP 404 response
        const char* response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 134\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html>\r\n"
            "<head><title>404 Not Found</title></head>\r\n"
            "<body>\r\n"
            "<h1>404 Not Found</h1>\r\n"
            "<p>The requested URL was not found on this server.</p>\r\n"
            "</body>\r\n"
            "</html>";
        send(client_socket, response, strlen(response), 0);
    } else if (path == "[AUTOINDEX]") {
        // Generate and send a directory listing
        // send_autoindex_response(client_socket, uri);
    } else {
        // Serve the file
        // serve_file(client_socket, path);
        const char* response =
            "HTTP/1.1 yehh boiiii\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 134\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html>\r\n"
            "<head><title>wbserv</title></head>\r\n"
            "<body>\r\n"
            "<h1>c comment frro</h1>\r\n"
            "<p>The requested URL was found on this server +</p>\r\n"
            "</body>\r\n"
            "</html>";
        send(client_socket, response, strlen(response), 0);
    }

    close(client_socket); // Close the client socket after sending the response
}

void Webserv::start(void)
{
    std::vector<pollfd> poll_fds;
    std::map<int, ServerConfig*> client_fd_to_server; // client_socket -> svconfig
    std::map<int, ServerConfig*> fd_to_server; // server_socket -> svconfig


    // Register all server sockets
    for (size_t i = 0; i < this->servers.size(); ++i)
    {
        pollfd pfd;
        pfd.fd = this->servers[i].server_socket;
        pfd.events = POLLIN;
        poll_fds.push_back(pfd);
        fd_to_server[this->servers[i].server_socket] = &this->servers[i];
    }

    std::cout << "\033[92m ===== STARTED " << poll_fds.size() << " SERVERZ ===== \033[0m" << std::endl;

    // poll() loop on all servers
    while (true)
    {
        int ret = poll(poll_fds.data(), poll_fds.size(), -1);
        if (ret == -1) {
            perror("poll"); // cant poll ?
            break;
        }

        for (size_t i = 0; i < poll_fds.size(); ++i)
        {
            struct pollfd& pfd = poll_fds[i];            
            
            // nothing....
            if (!(pfd.revents & POLLIN))
                continue;

            // accept new [connection]       
            if (fd_to_server.count(pfd.fd))
            {
                ServerConfig* serv = NULL;
                std::map<int, ServerConfig*>::iterator  it = fd_to_server.find(pfd.fd);
                if (it != fd_to_server.end() && it->second != NULL)
                    serv = it->second;
                else
                    std::cerr << "Error: no valid ServerConfig found for [fd]: " << pfd.fd << std::endl;
            

                int client_fd = accept(pfd.fd,
                    (struct sockaddr*)&(serv->client_addr), &serv->client_addr_len
                );

                if (client_fd < 0) {
                    std::cerr << "Error accepting connection on port "<< serv->port << ": " << strerror(errno) << std::endl;
                    continue;
                }

                // add new client socket to poll
                pollfd client_pfd;

                client_pfd.fd = client_fd;
                client_pfd.events = POLLIN;
                poll_fds.push_back(client_pfd);
                client_fd_to_server[client_fd] = serv;

            }
            // handle [connection]
            else
            {
                // client socket: Handle client
                ServerConfig* serv = client_fd_to_server[pfd.fd];
                this->handle_client(serv->client_socket, *serv);
                
                if (true) {
                    int client_fd = pfd.fd;  // Add this at the start of the else branch
                    close(client_fd);

                    // Remove from poll_fds
                    client_fd_to_server.erase(client_fd);

                    std::cout << "Successfully closed connection (fd: " << client_fd << ")" << std::endl;
                }
                
            }
        }
    }
    for(uint32_t i = 0; i < this->servers.size(); i ++)
        close(this->servers[i].server_socket);
}