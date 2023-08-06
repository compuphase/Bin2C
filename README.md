# Bin2C - convert a binary file to a C array.

Bin2C is a simple utility for converting a binary file to a C array declaration,
which can then be included within an application.

Usage:

```
bin2c input_file [output_file] [options]
```

## Command line arguments

The `input_file` parameter is required.

The name (and path) of the `output_file` may be explicitly set. If not specified,
the output file has the same name as the input file, but with the extension `.h`.

Various options are available:

| Short     | Long         | Description |
|-----------|---------------|-------------|
| -a        | --append      | Append to the output file instead of overwriting it. |
| -b number | --bits number | Set the width in bits of the array elements. This can be 8, 16 or 32 (for `uint8_t`, `uint16_t` or `uint32_t` respectively). The default bit size = 8. |
| -d        | --define      | Declare the array size as a #define (default is to declare it as a "const unsigned int" variable). |
| -h        | --help        | Show brief help. |
| -l name   | --label name  | Set the symbol name for the array. If not specified, the symbol name is the input filename, without extension or path. However, if the filename is not a valid symbol name, this option must be used to set the symbol name explicitly. |
| -m        | --mutable     | Declare the array as mutable (non-const). |
| -t        | --text        | Open the input file as a text file (Microsoft Windows only; this esssentially translates CR-LF pairs in the input file to LF). |
| -z        | --zero        | Append a zero terminator byte at the end of the array. |

For example, using:
```
bin2c my_file.dat --label data
```

will create a file called `my_file.h` with contents along the lines of:

```c
const uint8_t data[3432] = {
   0x43, 0x28, 0x41, 0x11, 0xa3, 0xff,
   ...
   0x00, 0xff, 0x23
};

const int data_length = 3432;
```

This can then be used within your application, for example with SDL you would
use SDL_RWops. The application can also be used in a very similar fashion to
Qt's RC system.

The purpose of the `--zero` option is that it allows you to embed a text file
in a C program. Then, the generated array may be manipulated as a string (text
files do not have zero bytes). In Linux and Unix-like operating systems, lines
in a text files end with a "newline" (which is the LF character). In Microsoft
Windows, lines end with CR-LF pairs. The CR characters are redundant in the
generated array, in most cases. Therefore, you may have these stripped with the
`--text` option.

For binary formats, the alignment may be important. A C/C++ compiler makes sure
that integers are aligned on a multiple of the integer size. Thus, a simple and
portable way to ensure that the generated array is 32-bit aligned, is to dump
the data as 32-bit integers. This is the purpose of the `--bits` option. Note
that the multi-byte values are in Little Endian, so that the byte order is the
same as for byte-sized fields.

In the typical case where you embed binary data inside a C/C++ program, you will
use the data as-is. That is, you will not modify it. The default action of Bin2C,
to declare the array as "const" is what you want. As a side benefit, when the
code is for a microcontroller, the array will end up in Flash ROM, and not in
SRAM (and microcontrollers typically have more Flash memory than SRAM). However,
in some cases, you may want the data to be changeable by the code that you embed
it in. To this end, use the option `--mutable`.

## Building Bin2C

I haven't included a Makefile because the utility is SO simple, I don't
think that one is needed. But for an example, compiling for GNU/Linux can be
done as shown

```
gcc -o bin2c bin2c.c
```

In the current system, you can tell bin2c to compress the data with BZ2
compression. This would be very useful in applications where a lot of files
are stored this way or if memory is tight (although not CPU). To produce an
executable which can make bz2 files, define USE_BZ2. However, since this is
such a simple application, you can either define USE_BZ2 or not and it will
then produce compressed data or not. An example as to how to compile a BZ2
compression version of bin2c is as such

```
gcc -o bin2cbz2 bin2c.c -DUSE_BZ2 -lbz2
```

This will add an extra constant, data_length_uncompressed, which is the size
of the file before it was compressed. So to decompress the file, you would
do something like the following:

```c
unsigned int decompressed_size = data_length_uncompressed;
char *buf = malloc(data_length_uncompressed);
int status;

status = BZ2_bzBuffToBuffDecompress(buf, &decompressed_size,
        const_cast<char *>data, (unsigned int)data_length, 0, 0);

// do something with buf
free(buf);
```

I'm not entirely happy with having to do const_cast in C++ so if anyone can
suggest an alternative then I'd be happy to implement it.

## In closing

Patches are welcome, just fork the project on github and send me a pull
request. If you are unable or unwilling to do this through github, then feel
free to email me your patch. This utility is so small I don't think that any
licence is needed, and I took most of the code from Serge Fukanchick and made
quite a few modifications so left it in the public domain. So please just send
me a little note to say that you don't mind your code being in the public
domain.

