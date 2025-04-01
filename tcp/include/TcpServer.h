﻿#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <functional>
#include <set>

#include <thread>
#include <mutex>
#include <shared_mutex>

#ifdef _WIN32 // Windows NT
#else // *nix

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#include "general.h"

/// Sipmple TCP
namespace stcp {

struct KeepAliveConfig{
  ka_prop_t ka_idle = 120;
  ka_prop_t ka_intvl = 3;
  ka_prop_t ka_cnt = 5;
};

struct TcpServer {
  struct Client : public TcpClientBase {
    friend struct TcpServer;

    std::mutex access_mtx;
    SocketAddr_in address;
    Socket socket;
    status _status = status::connected;


  public:
    Client(Socket socket, SocketAddr_in address);
    virtual ~Client() override;
    virtual uint32_t getHost() const override;
    virtual uint16_t getPort() const override;
    virtual status getStatus() const override {return _status;}
    virtual status disconnect() override;

    virtual DataBuffer loadData() override;
    virtual bool sendData(const void* buffer, const size_t size) const override;
    virtual SocketType getType() const override {return SocketType::server_socket;}
  };

  struct ClientKey{ uint32_t host; uint16_t port; };

  struct ClientComparator {
    using is_transparent = std::true_type;

    bool operator()(const std::unique_ptr<Client>& lhs, const std::unique_ptr<Client>& rhs) const {
      return (uint64_t(lhs->getHost()) | uint64_t(lhs->getPort()) << 32) < (uint64_t(rhs->getHost()) | uint64_t(rhs->getPort()) << 32);
    }

    bool operator()(const std::unique_ptr<Client>& lhs, const ClientKey& rhs) const {
      return (uint64_t(lhs->getHost()) | uint64_t(lhs->getPort()) << 32) < (uint64_t(rhs.host) | uint64_t(rhs.port) << 32);
    }

    bool operator()(const ClientKey& lhs, const std::unique_ptr<Client>& rhs) const {
      return (uint64_t(lhs.host) | uint64_t(lhs.port) << 32) < (uint64_t(rhs->getHost()) | uint64_t(rhs->getPort()) << 32);
    }
  };

  class ClientList;
  typedef std::function<void(DataBuffer, Client&)> handler_function_t;
  typedef std::function<void(Client&)> con_handler_function_t;
  static constexpr auto default_data_handler = [](DataBuffer, Client&){};
  static constexpr auto default_connsection_handler = [](Client&){};

  enum class status : uint8_t {
    up = 0,
    err_socket_init = 1,
    err_socket_bind = 2,
    err_scoket_keep_alive = 3,
    err_socket_listening = 4,
    close = 5
  };

private:
  Socket serv_socket;
  uint16_t port;
  status _status = status::close;
  handler_function_t handler = default_data_handler;
  con_handler_function_t connect_hndl = default_connsection_handler;
  con_handler_function_t disconnect_hndl = default_connsection_handler;

  ThreadPool thread_pool;
  typedef std::set<std::unique_ptr<Client>, ClientComparator>::iterator ClientIterator;
  KeepAliveConfig ka_conf;
  std::set<std::unique_ptr<Client>, ClientComparator> client_list;
  std::mutex client_mutex;

  bool enableKeepAlive(Socket socket);
  void handlingAcceptLoop();
  void waitingDataLoop();

public:
  TcpServer(const uint16_t port,
            KeepAliveConfig ka_conf = {},
            handler_function_t handler = default_data_handler,
            con_handler_function_t connect_hndl = default_connsection_handler,
            con_handler_function_t disconnect_hndl = default_connsection_handler,
            unsigned int thread_count = std::thread::hardware_concurrency()
            );

  ~TcpServer();

  //! Set client handler
  void setHandler(handler_function_t handler);

  ThreadPool& getThreadPool() {return thread_pool;}

  // Server properties getters
  uint16_t getPort() const;
  uint16_t setPort(const uint16_t port);
  status getStatus() const {return _status;}

  // Server status manip
  status start();
  void stop();
  void joinLoop();

  // Server client management
  bool connectTo(uint32_t host, uint16_t port, con_handler_function_t connect_hndl);
  void sendData(const void* buffer, const size_t size);
  bool sendDataBy(uint32_t host, uint16_t port, const void* buffer, const size_t size);
  bool disconnectBy(uint32_t host, uint16_t port);
  void disconnectAll();
};

}

#endif // TCPSERVER_H
