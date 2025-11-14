
/*
 * Header files contain declarations, not definitions, of classes,
 * functions, and variables. Similar to an interface in c#/java.
 */

 #include <string>
 #include <netinet/in/h>
 #include "queue.h"

 // header guards for checking if macro is already defined
 #ifndef INCLUDED_HTTP_TCPSERVER_LINUX
 #define INCLUDED_HTTP_TCPSERVER_LINUX

 namespace http
 {
    class TcpServer
    {
        public:
        TcpServer(std::string ip_address, int port);
        ~TcpServer();
        void close_server();
        int init_server();
        void accept_loop(ConnectionQueue::Queue &queue, std::atomic<bool> &running);
        void handle_client(int* client_socket);

        private:
        std::string m_ip_address;
        unsigned int m_port;
        int m_socket;
        int m_new_socket;
        unsigned long m_incoming_message;
        struct sockaddr_in m_socket_address;
        unsigned int m_socket_address_len;
        std::string m_server_message;

        std::string build_response();
        void start_listen();
        void accept_connections();
        void read_request();
        void send_response();
    };
    // namespace http
 }
 
 #endif