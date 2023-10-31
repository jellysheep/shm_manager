from pybind11.setup_helpers import Pybind11Extension
from setuptools import setup

ext_modules = [
    Pybind11Extension(
        'shm_manager',
        ['src/shm_manager_pybind.cpp'],
        extra_link_args=['-lrt'],
        extra_compile_args=['-std=c++17'],
    ),
]

setup(name='shm_manager',
      version='0.0.1',
      description='Shared memory manager handling a list of non-persistent memory regions provided over abstract unix sockets',
      author='Max Mertens',
      author_email='max.mail@dameweb.de',
      license='BSD-3-Clause',
      zip_safe=False,
      python_requires='>=3.6',
      ext_modules=ext_modules,
      include_package_data=True,
      install_requires=[
          'pybind11',
      ])
