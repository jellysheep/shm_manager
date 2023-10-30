import time
from multiprocessing import Process

from lib import shm_manager

BUFNAME = 'my_buffer'


def manager_process():
    manager = shm_manager.ShmManager()
    manager.run()


def client_process_1():
    client = shm_manager.ShmClient.create(BUFNAME, 1024)
    client.map_fd()
    print(f'client 1: fd is {client.fd}, address is {client.addr}')


def client_process_2():
    client = shm_manager.ShmClient.get(BUFNAME)
    shm_manager.ShmClient.remove(BUFNAME)
    client.map_fd()
    print(f'client 2: fd is {client.fd}, address is {client.addr}')


def __main__():
    manager = Process(target=manager_process)
    manager.start()
    time.sleep(0.1)

    client1 = Process(target=client_process_1)
    client1.start()
    client1.join()

    client2 = Process(target=client_process_2)
    client2.start()
    client2.join()

    shm_manager.ShmClient.send_quit()
    manager.join()


if __name__ == '__main__':
    __main__()
