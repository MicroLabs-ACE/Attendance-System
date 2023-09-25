cdef extern from "print_name.py":
    void print_input_name(str name)

def print_name_from_cython(str name):
    print_input_name(name)
