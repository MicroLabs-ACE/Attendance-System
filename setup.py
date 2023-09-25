from setuptools import setup
from Cython.Build import cythonize

setup(
    name="print_name_cython",
    ext_modules=cythonize("print_name_cython.pyx"),
)
