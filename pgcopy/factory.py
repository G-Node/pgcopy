
import errors
import bin_writer

def create_writer(file_handle=None, binary=True):
    if not binary:
        raise errors.UnsupportedError("Only binary format supported currently")
    if not file_handle:
        file_handle = open ("/dev/stdout", "wb")

    writer = bin_writer.PgCopyBinWriter (file_handle, close_handle=True)
    return writer
