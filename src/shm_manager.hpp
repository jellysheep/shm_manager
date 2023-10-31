#ifndef SHMMANAGER_SHARED_HPP
#define SHMMANAGER_SHARED_HPP

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

#define SHMMANAGER_DEBUG 0

namespace shm_manager
{
constexpr size_t SOCKET_PATH_SIZE = 8;
constexpr std::array<char, SOCKET_PATH_SIZE> socket_path = { 0, 's', 'h', 'm', '_', 'm', 'a', 'n' };

constexpr size_t MAX_NAME_SIZE = 80;
using nametype = std::array<char, MAX_NAME_SIZE>;

enum class RequestMode : uint8_t
{
  CREATE,
  GET,
  REMOVE,
  QUIT,
};

struct Request
{
  RequestMode mode;
  nametype name;
  std::size_t size;
};

namespace utils
{

static int send_request(const Request& req)
{
#if SHMMANAGER_DEBUG
  std::cout << "client: preparing request of mode " << (int)req.mode << std::endl;
#endif
  int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (client_socket == -1)
  {
    throw std::runtime_error("client: error creating socket");
  }
#if SHMMANAGER_DEBUG
  std::cout << "client: socket is " << client_socket << std::endl;
#endif

  struct sockaddr_un server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sun_family = AF_UNIX;
  std::memcpy(server_addr.sun_path, socket_path.data(), SOCKET_PATH_SIZE);

  if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
  {
    throw std::runtime_error("client: error connecting socket");
  }
  ssize_t len = send(client_socket, &req, sizeof(Request), 0);
  if (len != sizeof(Request))
  {
    throw std::runtime_error("client: error sending request");
  }
  return client_socket;
}

static int receive_ret(int client_socket)
{
  int ret;
  size_t len = recv(client_socket, &ret, sizeof(int), 0);
  if (len != sizeof(int))
  {
    throw std::runtime_error("client: error receiving ret");
  }
#if SHMMANAGER_DEBUG
  std::cout << "client: received return " << ret << std::endl;
#endif
  if (close(client_socket) != 0)
  {
    throw std::runtime_error("client: error closing socket");
  }
  return ret;
}

static int receive_fd(int client_socket)
{
  char nothing[1];
  char buf[128];
  struct iovec nothing_ptr;
  nothing_ptr.iov_base = nothing;
  nothing_ptr.iov_len = 1;
  struct msghdr msg;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = &nothing_ptr;
  msg.msg_iovlen = 1;
  msg.msg_flags = 0;
  msg.msg_control = buf;
  msg.msg_controllen = sizeof(struct cmsghdr) + sizeof(int);
  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg == nullptr)
  {
    throw std::runtime_error("client: error creating receiving cmsg");
  }
  cmsg->cmsg_len = msg.msg_controllen;
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;

  ssize_t len = recvmsg(client_socket, &msg, 0);
  if (len == -1)
  {
    throw std::runtime_error("client: error receiving fd");
  }
  if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
  {
    throw std::runtime_error("client: received wrong cmsg data");
  }
#if SHMMANAGER_DEBUG
  std::cout << "client: got msg of size " << len << std::endl;
  std::cout << "client: got msg with ctrllen " << msg.msg_controllen << std::endl;
#endif

  int fd = *(int*)CMSG_DATA(cmsg);
#if SHMMANAGER_DEBUG
  std::cout << "client: received fd " << fd << std::endl;
#endif

  cmsg = CMSG_NXTHDR(&msg, cmsg);
  if (cmsg != nullptr)
  {
    throw std::runtime_error("client: received more than one fd");
  }
  return fd;
}
}  // namespace utils

class ShmManager
{
  friend class ShmClient;

  ShmManager(const ShmManager&) = delete;
  ShmManager& operator=(const ShmManager&) = delete;
  ShmManager(ShmManager&& other) = delete;
  ShmManager& operator=(ShmManager&& other) = delete;

  std::unordered_map<std::string, int> fd_map;

public:
  ShmManager()
  {
  }

  ~ShmManager()
  {
    for (const auto& [name, fd] : fd_map)
    {
#if SHMMANAGER_DEBUG
      std::cout << "manager: closing fd " << fd << std::endl;
#endif
      if (close(fd) != 0)
      {
        throw std::runtime_error("manager: error closing fd");
      }
    }
  }

  void run()
  {
    int server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
      throw std::runtime_error("manager: error creating socket");
    }

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    std::memcpy(server_addr.sun_path, socket_path.data(), SOCKET_PATH_SIZE);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
      throw std::runtime_error("manager: error binding socket");
    }
    if (listen(server_socket, 5) == -1)
    {
      throw std::runtime_error("manager: error listening on socket");
    }

#if SHMMANAGER_DEBUG
    std::cout << "manager: listening ..." << std::endl;
#endif
    while (true)
    {
      struct sockaddr_un client_addr;
      socklen_t client_addr_len = sizeof(client_addr);
      int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
      if (client_socket == -1)
      {
        throw std::runtime_error("manager: error accepting client connection");
      }
#if SHMMANAGER_DEBUG
      std::cout << "manager: got connection" << std::endl;
#endif
      try
      {
        bool quit = handle_client(client_socket);
        if (quit)
        {
          break;
        }
      }
      catch (const std::runtime_error& e)
      {
        std::cerr << "manager: error while handling client: " << e.what() << std::endl;
      }
    }
#if SHMMANAGER_DEBUG
    std::cout << "manager: quitting" << std::endl;
#endif
    if (close(server_socket) != 0)
    {
      throw std::runtime_error("manager: error closing server socket");
    }
  }

  bool handle_client(int client_socket)
  {
    char buffer[1024];
    ssize_t len = recv(client_socket, buffer, sizeof(buffer), 0);
    if (len != sizeof(Request))
    {
      // this is probably due to wait_for_manager()
      // throw std::runtime_error("manager: error receiving client request");
      return false;
    }
    Request req = *(Request*)buffer;
#if SHMMANAGER_DEBUG
    std::cout << "manager: got request with mode " << (int)req.mode << std::endl;
#endif

    std::string name{ req.name.data(), req.name.size() };
    if (req.mode == RequestMode::CREATE)
    {
#if SHMMANAGER_DEBUG
      std::cout << "manager: creating anonymous memory" << std::endl;
#endif
      int fd = memfd_create("shm_manager", 0);
      if (ftruncate(fd, req.size) != 0)
      {
        throw std::runtime_error("manager: error on ftruncate");
      }
      fd_map.emplace(name, fd);
      req.mode = RequestMode::GET;
    }
#if SHMMANAGER_DEBUG
    std::cout << "manager: fd entries: " << fd_map.size() << ", query is '" << name << "'" << std::endl;
#endif
    if (req.mode == RequestMode::GET)
    {
      int fd = fd_map.at(name);
#if SHMMANAGER_DEBUG
      std::cout << "manager: sending fd " << fd << std::endl;
#endif

      struct iovec nothing_ptr;
      nothing_ptr.iov_base = (void*)"!";
      nothing_ptr.iov_len = 1;
      struct msghdr msg;
      msg.msg_name = nullptr;
      msg.msg_namelen = 0;
      msg.msg_iov = &nothing_ptr;
      msg.msg_iovlen = 1;
      msg.msg_flags = 0;
      msg.msg_control = buffer;
      msg.msg_controllen = sizeof(struct cmsghdr) + sizeof(int);
      struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
      cmsg->cmsg_len = msg.msg_controllen;
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
      *(int*)CMSG_DATA(cmsg) = fd;
#if SHMMANAGER_DEBUG
      std::cout << "manager: sending cmsg with controllen " << msg.msg_controllen << std::endl;
#endif

      if (sendmsg(client_socket, &msg, 0) == -1)
      {
        throw std::runtime_error("manager: error sending fd");
      }
      if (close(client_socket) != 0)
      {
        throw std::runtime_error("manager: error closing client socket");
      }
    }
    else
    {
      if (req.mode == RequestMode::REMOVE)
      {
#if SHMMANAGER_DEBUG
        std::cout << "manager: removing '" << name << "'" << std::endl;
#endif
        auto it = fd_map.find(name);
        if (it == fd_map.end())
        {
          throw std::runtime_error("manager: could not find entry in fd_map");
        }
#if SHMMANAGER_DEBUG
        std::cout << "manager: closing fd " << it->second << std::endl;
#endif
        if (close(it->second) != 0)
        {
          throw std::runtime_error("manager: error closing fd");
        }
        fd_map.erase(it);
      }
      int ret = 0;
#if SHMMANAGER_DEBUG
      std::cout << "manager: sending return " << ret << std::endl;
#endif
      len = send(client_socket, &ret, sizeof(int), 0);
      if (len != sizeof(int))
      {
        throw std::runtime_error("manager: error sending return");
      }
      if (close(client_socket) != 0)
      {
        throw std::runtime_error("manager: error closing client socket");
      }
      if (req.mode == RequestMode::QUIT)
      {
        return true;
      }
    }
    return false;
  }
};

class ShmClient
{
  int fd;
  size_t size;
  void* addr;

  ShmClient(int fd, size_t size, void* addr) : fd{ fd }, size{ size }, addr{ addr } {};

  ShmClient() : ShmClient(-1, 0, nullptr){};

  ShmClient(const ShmClient&) = delete;
  ShmClient& operator=(const ShmClient&) = delete;

  static ShmClient get(const std::string& name, RequestMode mode, size_t size)
  {
    nametype name_arr{};
    std::memcpy(name_arr.data(), name.data(), std::min(name.size(), MAX_NAME_SIZE - 1));
    Request req{ mode, name_arr, size };
    int client_socket = utils::send_request(req);
    int fd = utils::receive_fd(client_socket);
    if (fd == -1)
    {
      throw std::runtime_error("client: error sending request");
    }
    struct stat fd_state = {};
    if (fstat(fd, &fd_state) != 0)
    {
      std::cerr << "error " << errno << std::endl;
      throw std::runtime_error("client: error on fstat()");
    }
    if (fd_state.st_size == 0)
    {
      throw std::runtime_error("client: received fd has zero length");
    }
    if (mode == RequestMode::CREATE && fd_state.st_size != (ssize_t)size)
    {
      throw std::runtime_error("client: received fd has wrong length");
    }
    size = fd_state.st_size;
    return { fd, size, nullptr };
  }

public:
  ShmClient(ShmClient&& other) : ShmClient()
  {
    *this = std::move(other);
  };

  ShmClient& operator=(ShmClient&& other)
  {
    std::swap(fd, other.fd);
    std::swap(size, other.size);
    std::swap(addr, other.addr);
    return *this;
  };

  ~ShmClient()
  {
    if (fd >= 0)
    {
#if SHMMANAGER_DEBUG
      std::cout << "client: closing fd " << fd << std::endl;
#endif
      if (close(fd) != 0)
      {
        throw std::runtime_error("client: error closing fd");
      }
    }
    if (addr)
    {
#if SHMMANAGER_DEBUG
      std::cout << "client: unmapping " << addr << std::endl;
#endif
      munmap(addr, size);
    }
  }

  template <class Predicate>
  static bool wait_for_manager(Predicate stop_waiting)
  {
    int client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
      throw std::runtime_error("client: error creating socket");
    }

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    std::memcpy(server_addr.sun_path, socket_path.data(), SOCKET_PATH_SIZE);

    do
    {
      if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0)
      {
        if (close(client_socket) != 0)
        {
          throw std::runtime_error("client: error closing socket");
        }
        return true;
      }
    } while (!stop_waiting());
    return false;
  }

  static ShmClient create(const std::string& name, size_t size)
  {
    return get(name, RequestMode::CREATE, size);
  }

  static ShmClient get(const std::string& name)
  {
    return get(name, RequestMode::GET, 0);
  }

  static void remove(const std::string& name)
  {
    nametype name_arr{};
    std::memcpy(name_arr.data(), name.data(), std::min(name.size(), MAX_NAME_SIZE - 1));
    Request req{ RequestMode::REMOVE, name_arr, 0 };
    int client_socket = utils::send_request(req);
    int ret = utils::receive_ret(client_socket);
    if (ret == -1)
    {
      throw std::runtime_error("client: error sending request");
    }
  }

  static void send_quit()
  {
    Request req{ RequestMode::QUIT, {}, 0 };
    int client_socket = utils::send_request(req);
    int ret = utils::receive_ret(client_socket);
    if (ret == -1)
    {
      throw std::runtime_error("client: error sending quit request");
    }
  }

  int get_fd() const
  {
    return fd;
  }

  void* get_addr() const
  {
    return addr;
  }

  void map_fd(void* target_addr = nullptr)
  {
    if (addr != nullptr)
    {
      throw std::runtime_error("client: address is already set");
    }
    int flags = MAP_SHARED;
    if (target_addr != nullptr)
    {
      flags |= MAP_FIXED_NOREPLACE;
    }
    void* new_addr = mmap(target_addr, size, PROT_READ | PROT_WRITE, flags, fd, 0);
    if (new_addr == MAP_FAILED || (target_addr != nullptr && addr != target_addr))
    {
      throw std::runtime_error("client: error mmap'ing fd to target address");
    }
    addr = new_addr;
#if SHMMANAGER_DEBUG
    std::cout << "client: mapped fd to " << addr << std::endl;
#endif
  }
};

}  // namespace shm_manager

#endif
