# BPS File Format Specification

_by byuu. Public domain._ https://byuu.org/

BPS patches encode the differences between two files.

    string "BPS1"
    number source-size
    number target-size
    number metadata-size
    string metadata[metadata-size]
    repeat {
      number action | ((length - 1) << 2)
      action 0: SourceRead {
      }
      action 1: TargetRead {
        byte[] length
      }
      action 2: SourceCopy {
        number negative | (abs(offset) << 1)
      }
      action 3: TargetCopy {
        number negative | (abs(offset) << 1)
      }
    }
    uint32 source-checksum
    uint32 target-checksum
    uint32 patch-checksum

## Synonyms

A few terms are used interchangeably as appropriate.

* source = original file (input)
* target = modified file (output)
* number = variable-length integer encoding

## Linear creation

Rather than encoding a list of changes to files (insert data here, delete data
here, modify data here, ...); beat patches encode the steps needed to create a
new file from an old file. That is to say, the target file starts off as an
empty, zero-byte file. The patch commands tell us how to write each and every
byte of the target file sequentially.

## Variable-length number encoding

Rather than limit the maximum file size supported to 16MB (24-bit) or 4GB
(32-bit), beat patches use a variable-length encoding to support any number of
bits, and thus, any possible file size.

The basic idea is that we encode the lowest seven bits of the number, and then
the eighth bit of each byte is a flag to say whether the full number has been
represented or not. If set, this is the last byte of the number. If not, then
we shift out the low seven bits and repeat until the number is fully encoded.

One last optimization is to subtract one after each encode. Without this, one
could encode '1' with 0x81 or 0x01 0x80, producing an ambiguity.

Decoding is the inverse of the above process.

Below are C++ implementations of this idea. Note that we are using uint64 for
the data type: this will limit beat patches created with these algorithms to
64-bit file sizes. If 128-bit integers were available, they could be used
instead. Of course, it's silly to even imagine patching a file larger than 16
exabytes, but beat does allow it.

**Encoding**

    void encode(uint64 data) {
      while(true) {
        uint8 x = data & 0x7f;
        data >>= 7;
        if(data == 0) {
          write(0x80 | x);
          break;
        }
        write(x);
        data--;
      }
    }

**Decoding**

    uint64 decode() {
      uint64 data = 0, shift = 1;
      while(true) {
        uint8 x = read();
        data += (x & 0x7f) * shift;
        if(x & 0x80) break;
        shift <<= 7;
        data += shift;
      }
      return data;
    }

--------------------------------------------------------------------------------

## Header

First, we have the file format marker, "BPS1". We then encode the source and
target file sizes. Next, we encode optional metadata. If no metadata is present,
store an encoded zero here (0x80 per above.) Otherwise, specify the length of
the metadata.

Note that officially, metadata should be XML version 1.0 encoding UTF-8 data,
and the metadata-size specifies the actual length. As in, there is no
null-terminator after the metadata. However, the actual contents here are
entirely domain-specific, so literally anything can go here and the patch will
still be considered valid.

## Transfer lengths

We store lengths as length - 1 to prevent ambiguities. There is no sense in
encoding a command that ultimately does nothing. This also slightly helps with
patch size reduction in some cases.

## Relative offsets

beat patches keep track of the current file offsets in both the source and
target files separately. Reading from either increments their respective offsets
automatically.

As such, offsets are encoded relatively to the current positions. These offsets
can move the read cursors forward or backward. To support negative numbers with
variable-integer encoding requires us to store the negative flag as the lowest
bit, followed by the absolute value (eg abs(-1) = 1)

Note, and this is very important, for obvious reasons you cannot read from
before the start or after the end of the file. Further, you cannot read beyond
the current target write output offsets, as that data is not yet available.
Attempting to do so instantly makes the patch invalid and will abort patching
entirely.

**outputOffset:** this is a value that starts at zero. Every time a byte is
written to the target file, this offset is incremented by one.

**sourceRelativeOffset:** this is a value that starts at zero. SourceCopy will
adjust this value by a signed amount, and then increment the value by one for
each read performed by said command. This value can never be less than zero, or
greater than or equal to the source file size.

**targetRelativeOffset:** this is a value that starts at zero. TargetCopy will
adjust this value by a signed amount, and then increment the value by one for
each read performed by said command. This value can never be less than zero, or
greater than or equal to the outputOffset.

## Repeat

Commands repeat until the end of the patch. This can be detected by testing the
patch read location, and stopping when offset() >= size() - 12. Where 12 is the
number of bytes in the patch footer.

    void action() {
      uint64 data = decode();
      uint64 command = data & 3;
      uint64 length = (data >> 2) + 1;
    }

## SourceRead

This command copies bytes from the source file to the target file. Since both
the patch creator and applier will have access to the entire source file, the
actual bytes to output do not need to be stored here.

This command is rarely useful in delta patch creation, and is mainly intended to
allow for linear-based patchers. However, at times it can be useful even in
delta patches when data is the same in both source and target files at the same
location.

    void sourceRead() {
      while(length--) {
        target[outputOffset] = source[outputOffset];
        outputOffset++;
      }
    }

## TargetRead

When a file is modified, new data is thus created. This command can store said
data so that it can be written to the target file. This time, the actual data is
not available to the patch applier, so it is stored directly inside the patch.

    void targetRead() {
      while(length--) {
        target[outputOffset++] = read();
      }
    }

## SourceCopy

This command treats the entire source file as a dictionary, similarly to LZ
compression. An offset is supplied to seek the sourceRelativeOffset to the
desired location, and then data is copied from said offset to the target file.

    void sourceCopy() {
      uint64 data = decode();
      sourceRelativeOffset += (data & 1 ? -1 : +1) * (data >> 1);
      while(length--) {
        target[outputOffset++] = source[sourceRelativeOffset++];
      }
    }

## TargetCopy

This command treats all of the data that has already been written to the target
file as a dictionary. By referencing already written data, we can optimize
repeated data in the target file that does not exist in the source file.

This can allow for efficient run-length encoding. For instance, say 16MB of
0x00s appear in a row in only the target file. We can use TargetRead to write a
single 0x00. Now we can use TargetCopy to point at this byte, and set the length
to 16MB-1. The effect will be that the target output size grows as the command
runs, thus repeating the data.

    void targetCopy() {
      uint64 data = decode();
      targetRelativeOffset += (data & 1 ? -1 : +1) * (data >> 1);
      while(length--) {
        target[outputOffset++] = target[targetRelativeOffset++];
      }
    }

## Footer

Checksum information appears at the bottom of the file. The idea is to allow a
patcher to calculate this information as the patch is being produced. The source
checksum verifies that the input file is correct, and the target checksum
verifies that the patch has been applied successfully. Finally, the patch itself
has a checksum. The patch checksum is the checksum of every byte before it. In
other words, it does not include the last four bytes for obvious reasons. This
ensures the patch itself has not been corrupted.

Note that checksums are stored in CRC32 format. The intention of checksums is to
verify against corruption and mistakenly incorrect files. The idea was to keep
the file format simple, so cryptographically secure hashes were not used here.
If security is a grave concern, SHA256 or better hashes can be stored in the
manifest data. Otherwise, beat is not the right file format for such uses. 
