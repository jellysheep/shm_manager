import time
from multiprocessing import Process

import structstore as stst

from lib import shm_manager

BUFNAME = 'my_buffer'


def manager_process():
    manager = shm_manager.ShmManager()
    manager.run()


def client_process_1():
    client = shm_manager.ShmClient.create(BUFNAME, 2048)
    store = stst.StructStoreShared(client.fd, init=True)
    store.foo = 'bar'


def client_process_2():
    client = shm_manager.ShmClient.get(BUFNAME)
    shm_manager.ShmClient.remove(BUFNAME)
    store = stst.StructStoreShared(client.fd, init=False)
    print(f'client 2: store is {store}')


def __main__():
    manager = Process(target=manager_process)
    manager.start()
    time.sleep(0.1)

    client1 = Process(target=client_process_1)
    client1.start()
    client1.join()
    print(f'client 1 exited with {client1.exitcode}')

    client2 = Process(target=client_process_2)
    client2.start()
    client2.join()
    print(f'client 2 exited with {client2.exitcode}')

    shm_manager.ShmClient.send_quit()
    manager.join()


if __name__ == '__main__':
    __main__()
