# ShmManager

Shared memory manager handling a list of anonymous memory regions. Clients can
request new or existing memory regions based on a string index, which are
provided by the manager as file descriptors sent over abstract unix sockets.

No persistent data structures or filesystem features are used. Thus, when all
involved processes exited (or died), the shared memory regions (and any leftover
sockets) are automatically cleaned up by the kernel (manual release is also
possible).

Requirements: C++17, Linux kernel >=3.17 (for `memfd_create`).

## Usage examples

### C++

Manager:

```c++
#include <shm_manager.hpp>
int main() {
    shm_manager::ShmManager().run();
    return 0;
}
```

Client 1:

```c++
#include <shm_manager.hpp>
int main() {
  auto client = shm_manager::ShmClient::create("my_buffer", 1024);
  client.map_fd();
  *(int*)client.get_addr() = 42;
  return 0;
}
```

Client 2:

```c++
#include <shm_manager.hpp>
int main() {
  auto client = shm_manager::ShmClient::get("my_buffer");
  // remove reference at manager:
  shm_manager::ShmClient::remove("my_buffer");
  client.map_fd();
  int data = *(int*)client.get_addr();
  std::cout << "client got data " << data << std::endl;
  return 0;
}
```

### Python

See `examples/example.py`.

This library works nicely together with [structstore](https://github.com/jellysheep/structstore):

```python
client = shm_manager.ShmClient.create('my_buffer', 2048)
store = stst.StructStoreShared(client.fd, init=True)
store.foo = 'bar'
```

For a complete example, see `examples/example_structstore.py`.

## Implementation details

Anonymous shared memory regions are created using `memfd_create`. File
descriptors are sent to clients over abstract unix sockets using `SCM_RIGHTS`.

## License

This repo is released under the BSD 3-Clause License. See LICENSE file for
details.
