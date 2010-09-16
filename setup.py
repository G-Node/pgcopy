#!/usr/bin/env python
"""Utils around PostgreSQL's COPY function to import data

pgcopy is a set of tools, i.e. functions and classes, build
to enable the import of (large amounts of) data into a 
PostgreSQL server by using the COPY function. 
"""

from distutils.core import setup, Extension
import setuptools
from distutils.extension import Extension
import numpy as np

classifiers = [
        'Development Status :: 4 - Beta',
        'Programming Language :: Python',
        'Programming Language :: C',
        'License :: OSI Approved :: GNU General Public License (GPL)',
        'Intended Audience :: Developers',
        'Topic :: Database']

bin_copy_ext = Extension('pgcopy.native',
                         include_dirs = [np.get_include()],
                         sources = ['native/pgcopy.c'])

setup (name             = 'PostgreSQLCopy',
       version          = '1.0',
       author           = 'Christian Kellner',
       author_email     = 'kellner@biologie.uni-muenchen.de',
       description      = __doc__.split("\n")[0],
       long_description = "\n".join(__doc__.split("\n")[2:]),
       classifiers      = classifiers,
       ext_modules      = [bin_copy_ext],
       packages         = ['pgcopy']
       )
