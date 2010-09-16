#include <Python.h>
#include <numpy/arrayobject.h>
#include <numpy/ndarrayobject.h>
#include <arpa/inet.h>
#include <stdio.h>

//#include <arrayobject.h>
//#include "numpy/npy_common.h"

static PyObject *PgCopyError;

typedef void * (*FlipByteFunc) (void *src, char *buffer);

inline static void *
flip_bytes_16 (void *src, char *buffer)
{
  uint16_t *i, *o;
	
  i = (uint16_t *) src;
  o = (uint16_t *) buffer;
  *o = htons (*i);
	
  return (void *) buffer;
}

inline static void *
flip_bytes_32 (void *src, char *buffer)
{
  uint32_t *i, *o;
	
  i = (uint32_t *) src;
  o = (uint32_t *) buffer;
  *o = htonl (*i);
	
  return (void *) buffer;
}

inline static void *
flip_bytes_64 (void *src, char *buffer)
{
  uint32_t *i, *o;
	
  /* BIF FIXME: TBD */
	
  i = (uint32_t *) src;
  o = (uint32_t *) buffer;
  *o = htonl (*i);

  fprintf (stderr, "Warning!  flip_bytes_64 unimplemented!\n");

  return (void *) buffer;
}

inline static void *
flip_bytes_dummy (void *src, char *buffer)
{
  fprintf (stderr, "Warning! Using dummy byte fillper!\n");
  return src;
}

static FlipByteFunc
get_byte_flipper (size_t s)
{
  if (s == 2)
    return flip_bytes_16;
  else if (s == 4)
    return flip_bytes_32;
  else if (s == 8)
    return flip_bytes_64;
  
  return flip_bytes_dummy;
}



static PyObject *
PyArray_ToPgBinary(PyArrayObject *self, NPY_ORDER order)
{
  npy_intp numbytes;
  Py_ssize_t totalbytes;
  npy_intp index;
  char *dptr;
  int elsize;
  uint32_t nelsize;
  PyObject *ret;
  PyArrayIterObject *it;
  PyObject *new;
  char buffer[8];
  FlipByteFunc byte_flipper;
  
  if (order == NPY_ANYORDER)
    order = PyArray_ISFORTRAN(self);
  
  numbytes = PyArray_NBYTES(self); 
  elsize = PyArray_ITEMSIZE (self);
  
  if (order == NPY_FORTRANORDER) {
    /* iterators are always in C-order */
    new = PyArray_Transpose(self, NULL);
    if (new == NULL) {
      return NULL;
    }
  } else {
    Py_INCREF(self);
    new = (PyObject *)self;
  }
  
  it = (PyArrayIterObject *)PyArray_IterNew(new);
  Py_DECREF(new);

  if (it == NULL) {
      return NULL;
  }

  totalbytes = numbytes + ((numbytes / elsize) * sizeof (uint32_t));
  ret = PyBytes_FromStringAndSize(NULL, (Py_ssize_t) totalbytes);

  if (ret == NULL) {
    Py_DECREF(it);
    return NULL;
  }
  
  dptr = PyBytes_AS_STRING(ret);
  index = it->size;
  nelsize = htonl (elsize);
  byte_flipper = get_byte_flipper (elsize);
  
  while (index--) {
    
    memcpy (dptr, &nelsize, 4);
    dptr += 4;
    
    memcpy (dptr, (*byte_flipper) (it->dataptr, buffer), elsize);
    dptr += elsize;
    PyArray_ITER_NEXT(it);
  }
  
  Py_DECREF(it);
  
  return ret;
}

static int
PyArray_ToPgCopyFile(PyArrayObject *self, FILE *fp)
{
  npy_intp numbytes;
  npy_intp index;
  int elsize;
  uint32_t nelsize;
  PyArrayIterObject *it;
  PyObject *new;
  char buffer[8];
  FlipByteFunc byte_flipper;
  size_t nwritten;
  //NPY_BEGIN_THREADS_DEF;
  
  numbytes = PyArray_NBYTES(self); 
  elsize = PyArray_ITEMSIZE (self);
  
  Py_INCREF(self);
  new = (PyObject *)self;
  
  it = (PyArrayIterObject *)PyArray_IterNew(new);
  Py_DECREF(new);
  if (it == NULL) {
    return -1;
  }
  
  index = it->size;
  nelsize = htonl (elsize);
  byte_flipper = get_byte_flipper (elsize);
  
  //NPY_BEGIN_THREADS;
  while (index--) {
    const void *dptr;
    
    nwritten = fwrite ((const void *) &nelsize, 4, 1, fp);
    if (nwritten != 1)
      {
	fprintf (stderr, "Warning: IO Error!\n  ");
      }

    dptr = (*byte_flipper) (it->dataptr, buffer);

    nwritten = fwrite ((const void *) dptr, (size_t) elsize, 1, fp);
    if (nwritten != 1)
      {
	fprintf (stderr, "Warning: IO Error! (2)\n");
      }
    
    PyArray_ITER_NEXT(it);
  }
  
  //NPY_END_THREADS;
  Py_DECREF(it);
  
  return 0;
}




static PyObject *
convert_array (PyObject *self, PyObject *args, PyObject *kwds)
{
    NPY_ORDER order = NPY_CORDER;
    PyArrayObject *np_arr;
    static char *kwlist[] = {"array", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", kwlist,
				     &PyArray_Type, &np_arr)) {
        return NULL;
    }
    return PyArray_ToPgBinary(np_arr, order);
}

static PyObject *
array_tofile_converted(PyObject *self, PyObject *args, PyObject *kwds)
{
    int ret;
    PyObject *file;
    FILE *fd;
	PyArrayObject *np_arr;
    static char *kwlist[] = {"array", "file", NULL};
	
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!O", kwlist,
									 &PyArray_Type, &np_arr,
                                     &file)) {
        return NULL;
    }
	
    if (PyBytes_Check(file) || PyUnicode_Check(file)) {
#if 0
        file = npy_PyFile_OpenFile(file, "wb");
        if (file == NULL) {
            return NULL;
        }
#else
		return NULL;
#endif
    }
    else {
        Py_INCREF(file);
    }
#if defined(NPY_PY3K)
    fd = npy_PyFile_Dup(file, "wb");
#else
    fd = PyFile_AsFile(file);
#endif
    if (fd == NULL) {
        PyErr_SetString(PyExc_IOError, "first argument must be a " \
                        "string or open file");
        Py_DECREF(file);
        return NULL;
    }
    ret = PyArray_ToPgCopyFile(np_arr, fd);
#if defined(NPY_PY3K)
    fclose(fd);
#endif
    Py_DECREF(file);
    if (ret < 0) {
        return NULL;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef PgCopyMethods[] = {

    {"convert_array", (PyCFunction) convert_array, METH_VARARGS | METH_KEYWORDS,
     "Convert a NumPy Array to PostgeSQL COPY binary format"},
    {"array_tofile_converted", (PyCFunction) array_tofile_converted, METH_VARARGS | METH_KEYWORDS,
     "Convert a NumPy Array to PostgeSQL COPY binary format and writes it to a file object"},

    {NULL, NULL, 0, NULL}        /* Sentinel */
};



PyMODINIT_FUNC
initnative(void)
{
    PyObject *m;

    m = Py_InitModule("native", PgCopyMethods);
    if (m == NULL)
      return;

    PyModule_AddStringConstant(m, "__doc__", "pgcopy native (C) functions");

    import_array ();

    PgCopyError = PyErr_NewException("native.error", NULL, NULL);
    Py_INCREF(PgCopyError);
    PyModule_AddObject(m, "error", PgCopyError);
}
