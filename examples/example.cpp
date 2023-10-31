#include "shm_manager.hpp"
#include <iostream>
#include <thread>

const std::string BUFNAME = "my_buffer";

void manager_process()
{
  auto manager = shm_manager::ShmManager();
  manager.run();
}

void client_process_1()
{
  auto client = shm_manager::ShmClient::create(BUFNAME, 1024);
  client.map_fd();
  int data = 42;
  std::cout << "client 1 got data " << data << std::endl;
  *(int*)client.get_addr() = data;
}

void client_process_2()
{
  auto client = shm_manager::ShmClient::get(BUFNAME);
  // remove reference at manager:
  shm_manager::ShmClient::remove(BUFNAME);
  client.map_fd();
  int data = *(int*)client.get_addr();
  std::cout << "client 2 got data " << data << std::endl;
}

int main()
{
  auto manager = std::thread(manager_process);

  shm_manager::ShmClient::wait_for_manager([]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return false;
  });

  auto client1 = std::thread(client_process_1);
  client1.join();

  auto client2 = std::thread(client_process_2);
  client2.join();

  shm_manager::ShmClient::send_quit();
  manager.join();

  return 0;
}
