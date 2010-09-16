#!/usr/bin/env python

import os
import struct
import numpy
import binascii
import sys
import time
import native

import errors

__author__ = "Christian Kellner"

type_map = {'int16' : 21, 'int32' : 23, 'int64' : 20, 'float64' : 701}

def array_dim_lengths(a):
    l = (a.size, )
    return l

class PgCopyBinWriter(object):
    signature = 'PGCOPY\n\377\r\n\0'

    def writeBool (self, val):
        data = ["\x00", "\x01"][val]
        self.writeData (data)
    
    def writeInt16 (self, num):
        data = struct.pack (">ih", 2, num)
        self.writeData (data)

    def writeInt32 (self, num):
        data = struct.pack (">ii", 4, num)
        self.writeData (data)

    def writeInt64 (self, num):
        data = struct.pack (">iq", 8, num)
        self.writeData (data)

    def writeFloat32(self, num):
        data = struct.pack(">f", num)
        self.writeData (data)

    def writeFloat64 (self, num):
        data = struct.pack (">d", num)
        self.writeData (data)
        
    def writeNdArrayOLD(self, a):
        a.dtype.itemsize
        has_null = False
        dim_lengths = array_dim_lengths(a)
        type_id = type_map[a.dtype.name]
        data = struct.pack(">iii", len(dim_lengths), has_null, type_id)
        for i in dim_lengths:
            data += struct.pack(">ii", i, 1) #dim, lower_bound
        size = len (data) + a.nbytes + a.size * 4
        data = struct.pack (">i", size) + data
        self.writeData (data)
        tic = time.time ()
        for n in a:
            self.writeInt16 (n)
        tac = time.time ()
        sys.stderr.write ("formatting array took %f\n" % (tac - tic))

    def writeNdArray(self, a):
        a.dtype.itemsize
        has_null = False
        dim_lengths = array_dim_lengths(a)
        type_id = type_map[a.dtype.name]
        data = struct.pack(">iii", len(dim_lengths), has_null, type_id)
        for i in dim_lengths:
            data += struct.pack(">ii", i, 1) #dim, lower_bound
        size = len (data) + a.nbytes + a.size * 4
        data = struct.pack (">i", size) + data
        self.writeData (data)
        tic = time.time ()
        a.byteswap (True)
        buf = a.tostring ()
        h = struct.pack (">i", 2)
        wd = self.writeData
        for n in xrange (0, len (buf), 2):
            b = h + buf[n] + buf[n + 1]
            wd (b)
        tac = time.time ()
        sys.stderr.write ("formatting new array took %f\n" % (tac - tic))
    
    def writeNdArrayC(self, a):
        a.dtype.itemsize
        has_null = False
        dim_lengths = array_dim_lengths(a)
        type_id = type_map[a.dtype.name]
        data = struct.pack(">iii", len(dim_lengths), has_null, type_id)
        for i in dim_lengths:
            data += struct.pack(">ii", i, 1) #dim, lower_bound
        size = len (data) + a.nbytes + a.size * 4
        data = struct.pack (">i", size) + data
        self.writeData (data)
        tic = time.time ()
        s = native.convert_array (a)
        self.writeData (s)
        tac = time.time ()
        sys.stderr.write ("formatting array with C took %f\n" % (tac - tic))
    
    def writeNdArrayCF(self, a):
        a.dtype.itemsize
        has_null = False
        dim_lengths = array_dim_lengths(a)
        type_id = type_map[a.dtype.name]
        data = struct.pack(">iii", len(dim_lengths), has_null, type_id)
        for i in dim_lengths:
            data += struct.pack(">ii", i, 1) #dim, lower_bound
        size = len (data) + a.nbytes + a.size * 4
        data = struct.pack (">i", size) + data
        self.writeData (data)
        #tic = time.time ()
        native.array_tofile_converted (a, self.fd)
        #tac = time.time ()
        #sys.stderr.write ("writing converted array with CF took %f\n" % (tac - tic))
    
    def __init__(self, fd, close_handle=False, flags=0, header_ext=0):
        self.fd = fd
        self.flags = flags
        self.header_ext = header_ext
	self.close_handle = close_handle
        self.writer_map = {tuple : self.writeTuple,
                           int : self.writeInt32,
                           numpy.bool : self.writeBool,
                           numpy.int16 : self.writeInt16,
                           numpy.int32 : self.writeInt32,
                           numpy.int64 : self.writeInt64,
                           numpy.float32 : self.writeFloat32,
                           numpy.float64 : self.writeFloat64,
                           numpy.ndarray : self.writeNdArrayCF}

    def writeData(self, data):
        self.fd.write (data)
  
    def writeHeader(self):
        flags_data = struct.pack (">ii", self.flags, self.header_ext)
        header = PgCopyBinWriter.signature + flags_data
        self.writeData (header)

    def writeTuple(self, tu):
        data = struct.pack (">h", len (tu))
        self.writeData (data)
        for field in tu:
            self.write (field)
    
    def write(self, data):
        data_type = type (data)
        if not self.writer_map.has_key (data_type):
            raise TypeUnsupported("type %r not supported" % data_type)
        writer = self.writer_map[data_type]
        writer (data)

    def start(self):
        self.writeHeader()
        
    def finish(self):
        data = struct.pack (">h", -1)
        self.writeData (data)
        if self.close_handle:
            self.fd.close ()
