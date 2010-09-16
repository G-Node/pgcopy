__doc__ = """Utils around PostgreSQL's COPY function to import data

pgcopy is a set of tools, i.e. functions and classes, build
to enable the import of (large amounts of) data into a 
PostgreSQL server by using the COPY function. 
"""

import native
import bin_writer
import errors
from factory import create_writer
