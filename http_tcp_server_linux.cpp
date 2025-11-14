#include <sys/socket.h> // for socket(), AF_INET, SOCK_STREAM
#include <netinet/in.h> // for sockaddr_in
#include <string>
#include <iostream>

#include "http_tcp_server_linux.h"


namespace {
    void log(const std::string &message) {
        std::cout << message << std::endl;
    }

    void exit_with_error(const std::string &error_message) {
        std::cerr << "Error:  " << error_message << std::endl;
        exit(1);
    }
}

namespace http
{
    TcpServer::TcpServer(
        std::string ip_address,
        int port
    ) :
    m_ip_address(ip_address), m_port(port),
    m_socket(), m_new_socket(), m_incoming_message(),
    m_socket_address_len(sizeof(m_socket_address)) {
        init_server();
    }

    TcpServer::~TcpServer() {
        close(m_socket);
        close(m_new_socket);
    }

    int TcpServer::init_server() {
        /*
         * int socket(int domain, int type, int protocol);
         * Domain: Specifies communication domain by defining protocol
         * family socket will belong to. TCP/IP socket uses IPv4 defined
         * by AF_INET domain. (Address Family Internet)
         * 
         * Type: Describes communication structure the socket will allow
         * for protocol family. For reliable, full-duplex byte streams
         * we use SOCK_STREAM
         * 
         * Protocol: Specifies protocol the socket will use from given
         * family of protocols supporting chosen communication type.
         * For AF_INET only one protocol supports SOCK_STREAM.
         */
        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0) {
            exit_with_error("Cannot create socket.");
            return 1;
        }

        // name the socket
        m_socket_address.sin_family = AF_INET; // IPv4 address family
        /* htons => host To Network Short converts 16-bit number (port)
        from machines byte order (host order) to network byte order */
        m_socket_address.sin_port = htons(m_port); // convert port to network byte order
        
        /* inet_addr() expects c-style string, so we convert it.*/
        m_socket_address.sin_addr.s_addr = inet_addr(m_ip_address.c_str());

        // bind the socket -- assign IP address and port number to it
        if (bind(
            m_socket,
            (struct sockaddr*)&m_socket_address,
            m_socket_address_len) < 0) {
                /* casting the the socket_address to sockaddr from sockaddr_in is safe because it is a
                specialised version of sockaddr for IP comms - memory layout is compatible. */
                std::cerr << "Failed to bind socket to IP/Port" << std::endl;
                exit(1);
            }

            return 0;
    }

    void TcpServer::accept_loop(ConnectionQueue::Queue &queue, std::atomic<bool> &running) {
        start_listen();
        while (running) {
            fd_set readfds;                         /// file-descriptor set (bit array)
            FD_ZERO(&readfds);                      /// clear the fd set
            FD_SET(m_socket, &readfds);             /// add listening server socket to the set

            struct timeval timeout;
            timeout.tv_sec = 1;                     /// 1 second timeout
            timeout.tv_usec = 0;                    /// no micro seconds

            int activity = select(                  /// monitor activity on set of sockets for r, w, exception ///
                m_socket + 1;                       /// m_socket is a file-descriptor
                                                    /// highest numbered fd set in any of the sets plus one
                &readfds,                           /// pass fd set by reference
                nullptr,/                           /// not using write or error condition fd set in example
                nullptr,                        
                &timeout                            /// pass the timeout by reference
            );                      
        
            if (activity < 0 && errno != EINTR) {       /* activity return negative for an error.
                                                        * errno is a global variable set by system calls, from errno.h.
                                                        * Check this variable to get error code when activity < 0
                                                        * EINTR -- interrupted sys call constant defined in errno.h */
                std::cerr << "Error occurred in select()" << std::endl;
                break;   
            }

            // check that socket fd is ready
            if (activity > 0 && FD_ISSET(m_socket, &readfds)) {
                accept_connection(); // sets m_new_socket to be accept(m_socket, ...)
                int* client_socket = new int(m_new_socket);
                queue.enqueue(client_socket);
            }

        // when activity == 0, timeout occurred. Loop continues and checks running flag.
        }
    }

    void TcpServer::handle_client(int* client_socket) {
        m_new_socket = *client_socket;
        read_request();
        send_response();
        close(m_new_socket);
    }

    void TcpServer::start_listen() {
        // put socket in passive mode, wait for clients
        int number_of_connections_in_line = 1;
        if (listen(m_socket, number_of_connections_in_line) < 0) {
            std::cerr << "Socket failed to enter listening state" << std::endl;
            exit(1);
        }

        std::ostringstream ss; // allows use of << operator but not echoed to command line as with cout
        ss  << "\n*** Listening on ADDRESS: "
            << inet_ntoa(m_socket_address.sin_addr) // convert server ip addr from binary to human readable
            << " PORT: " << ntohs(m_socket_address.sin_port) // converts the port from network byte order to host byte order
            << " ***\n\n";
        log(ss.str());
    }

    void TcpServer::accept_connection() {
        // accept() returns a new socket descriptor for comms with the client
        m_new_socket = accept(
            m_socket,
            (struct sockaddr*)&m_socket_address,
            &m_socket_address_len
        );
        if (m_new_socket < 0) {
            std::ostringstream ss;
            ss  << "Server failed to accept incoming connection from ADDRESS: "
                << inet_ntoa(m_socket_adddress.sin_addr) << "; PORT: "
                << ntohs(m_socket_address.sin_port);
                exit_with_error(ss.str());
        }

    }

    void TcpServer::read_request() {
        // buffer used to store data transmitted in request to server
        const int BUFFER_SIZE = 30720; // 30KB network transfer

        char buffer[BUFFER_SIZE] = {0};
        int bytes_recieved = read(m_new_socket, buffer, BUFFER_SIZE);
        if (bytes_recieved < 0) {
            exit_with_error("Failed to read bytes from client socket connection.");
        }
    }

    void TcpServer::send_response() {
        std::string res = build_response();
        ssize_t bytes_sent = write(
            m_new_socket,
            res.c_str(), // convert to c style string
            res.size()
        );
        if (bytes_sent != static_cast<ssize_t>(res.size)) {
            log("Error sending response to client\n\n");
            log("Bytes sent: " + std::to_string(bytes_sent) + "\n");
            log("Response size: " + std::to_string(res.size()) + "\n");
            return;
        }
        log("----- Server sent response to client -----\n\n");
    }

    void TcpServer::close_server() {
        close(m_new_socket);
        close(m_socket);
    }
}

