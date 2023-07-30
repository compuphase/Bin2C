/*
 * This is bin2c program, which allows you to convert binary file to
 * C language array, for use as embedded resource, for instance you can
 * embed graphics or audio file directly into your program.
 * This is public domain software, use it on your own risk.
 * Contact Serge Fukanchik at fuxx@mail.ru  if you have any questions.
 *
 * Some modifications were made by Gwilym Kuiper (kuiper.gwilym@gmail.com)
 * I have decided not to change the licence.
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef USE_BZ2
#include <bzlib.h>
#endif

#if !defined _MAX_PATH
#   if defined MAX_PATH
#       define _MAX_PATH MAX_PATH
#   elif defined MAXPATH
#       define _MAX_PATH MAXPATH
#   else
#       define _MAX_PATH 260
#   endif
#endif

static void about(const char *arg)
{
    if (arg == NULL) {
        fprintf(stderr, "Bin2C converts a binary file to a C array declaration.\n\n"
                        "Usage: bin2c input_file [output_file] [options]\n\n"
                        "Command line arguments:\n"
                        "  input_file         The binary file to convert.\n"
                        "  output_file        The name of the generated file with the array declaration.\n\n");
    } else {
        fprintf(stderr, "ERROR: Invalid option '%s'.\n\n", arg);
    }
    fprintf(stderr, "Options:\n"
                    "  -a|--append        Append to the output file instead of overwriting.\n"
                    "  -b|--bits number   Set the width of the array elements (default = 8).\n"
                    "  -h|--help          Show brief help.\n"
                    "  -l|--label name    Set the symbol name for the array.\n"
                    "  -m|--macro         Declare the array size as a macro, instead of a variable.\n"
                    "  -t|--text          Open the input file as a text file (Windows only).\n"
                    "  -z|--zero          Append a zero terminator at the end of the array.\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    const char *f_inputname = NULL;
    char *f_outputname = NULL;
    char *symbolname = NULL;
    bool local_outputname = false;  /* name of output file was generated from input */
    bool local_symbolname = false;  /* name of C symbol was generated from input */

    bool is_appending = false;
    bool is_textfile = false;
    bool use_macro = false;
    bool zero_terminate = false;
    unsigned int bitsize = 8;

    /* parse command line */
    if (argc <= 1)
        about(NULL);
    for (int idx = 1; idx < argc; idx++) {
        if (argv[idx][0] == '-') {
            if (strcmp(argv[idx], "-a") == 0 || strcmp(argv[idx], "--append") == 0) {
                is_appending = true;
            } else if (strncmp(argv[idx], "-b", 2) == 0 || strncmp(argv[idx], "--bits", 6) == 0) {
                unsigned int j = (argv[idx][1] == '-') ? 6 : 2;
                if (isdigit(argv[idx][j]))
                    bitsize = atoi(&argv[idx][j]);
                else if (argv[idx][j] == '\0' && (idx + 1) < argc)
                    bitsize = atoi(argv[++idx]);
                else
                    about(argv[idx]); /* invalid option */
                if (bitsize != 8 && bitsize != 16 && bitsize != 32) {
                    fprintf(stderr, "ERROR: Invalid bit size (must be 8, 16 or 32).\n");
                    return 1;
                }
            } else if (strcmp(argv[idx], "-h") == 0 || strcmp(argv[idx], "--help") == 0) {
                about(NULL);
            } else if (strncmp(argv[idx], "-l", 2) == 0 || strncmp(argv[idx], "--label", 7) == 0) {
                unsigned int j = (argv[idx][1] == '-') ? 7 : 2;
                if (argv[idx][j] != '\0')
                    symbolname = &argv[idx][j];
                else if ((idx + 1) < argc)
                    symbolname = argv[++idx];
                else
                    about(argv[idx]); /* invalid option */
                if (!isalpha(*symbolname) && *symbolname != '_') {
                    fprintf(stderr, "ERROR: Invalid C symbol name '%s'.\n", symbolname);
                    return 1;
                }
            } else if (strcmp(argv[idx], "-m") == 0 || strcmp(argv[idx], "--macro") == 0) {
                use_macro = true;
            } else if (strcmp(argv[idx], "-t") == 0 || strcmp(argv[idx], "--text") == 0) {
                is_textfile = true;
            } else if (strcmp(argv[idx], "-z") == 0 || strcmp(argv[idx], "--zero") == 0) {
                zero_terminate = true;
            }
        } else {
            if (f_inputname == NULL) {
                f_inputname = argv[idx];
            } else if (f_outputname == NULL) {
                f_outputname = argv[idx];
            } else {
                fprintf(stderr, "ERROR: Too many filenames. Use 'bin2c --help' for usage information.\n");
                return 1;
            }
        }
    }

    /* test input, make default names for output and symbol name (if needed) */
    if (f_inputname == NULL) {
        fprintf(stderr, "ERROR: No input file. Use 'bin2c --help' for usage information.\n");
        return 1;
    }
    if (f_outputname == NULL) {
        char *ext;
        f_outputname = malloc(strlen(f_inputname) + 3); /* +2 for ".h" extension, +1 for '\0' */
        if (f_outputname == NULL) {
            fprintf(stderr, "ERROR: Memory allocation error.\n");
            return 1;
        }
        strcpy(f_outputname, f_inputname);
        ext = strrchr(f_outputname, '.');
        if (ext != NULL && strpbrk(f_outputname, "\\/") == NULL)
            *ext = '\0';    /* remove old extension */
        strcat(f_outputname, ".h");
        local_outputname = true;
    }
    if (symbolname == NULL) {
        const char *base;
        char *ext;
        base = f_inputname;
        while (strpbrk(base, "\\/") != NULL)
            base = strpbrk(base, "\\/") + 1;    /* skip all directory names */
        symbolname = malloc(strlen(base) + 1);  /* +1 for '\0' */
        if (symbolname == NULL) {
            fprintf(stderr, "ERROR: Memory allocation error.\n");
            return 1;
        }
        strcpy(symbolname, base);
        ext = strrchr(symbolname, '.');
        if (ext != NULL)
            *ext = '\0';    /* remove extension */
        local_symbolname = true;
    }

    FILE *f_input = fopen(f_inputname, is_textfile ? "rt" : "rb");
    if (f_input == NULL) {
        fprintf(stderr, "ERROR: Failed to open %s for reading.\n", f_inputname);
        return 1;
    }

    /* get the length of the input file, then read it fully in memory */
    fseek(f_input, 0, SEEK_END);
    unsigned int file_size = ftell(f_input);
    fseek(f_input, 0, SEEK_SET);
    if (zero_terminate)
        file_size += 1;
    uint8_t *buf = (uint8_t *)calloc(file_size, 1);
    if (buf == NULL) {
        fprintf(stderr, "ERROR: Memory allocation error.\n");
        return 1;
    }
    fread(buf, file_size, 1, f_input);
    fclose(f_input);

#ifdef USE_BZ2
    // allocate for bz2.
    unsigned int bz2_size =
      (file_size + file_size / 100 + 1) + 600; // as per the documentation

    uint8_t *bz2_buf = (uint8_t *) malloc(bz2_size);
    assert(bz2_buf);

    // compress the data
    int status =
      BZ2_bzBuffToBuffCompress(bz2_buf, &bz2_size, buf, file_size, 9, 1, 0);

    if (status != BZ_OK) {
        fprintf(stderr, "ERROR: Failed to compress data: error %i\n", status);
        return 1;
    }

    // and be very lazy
    free(buf);
    unsigned int uncompressed_size = file_size;
    file_size = bz2_size;
    buf = bz2_buf;
#endif

    FILE *f_output;
    if(is_appending)
        f_output = fopen(f_outputname, "a+t");
    else
        f_output = fopen(f_outputname, "wt");
    if (f_output == NULL) {
        fprintf(stderr, "ERROR: Failed to open %s for writing\n", f_outputname);
        return 1;
    }

    if (!is_appending)
        fprintf(f_output, "/* generated by Bin2C */\n"
                          "#include <stdint.h>");
    fprintf(f_output, "\n\n");
    assert(bitsize == 8 || bitsize == 16 || bitsize == 32);
    unsigned int array_size = (file_size + ((bitsize >> 3) - 1)) / (bitsize >> 3);
    fprintf(f_output, "const uint%u_t %s[%u] = {", bitsize, symbolname, array_size);
    bool need_comma = false;
    bool need_newline = true;
    uint32_t word = 0;
    int bits = 0;
    for (unsigned int idx = 0; idx < file_size; ++idx) {
        word |= (uint32_t)buf[idx] << bits;
        bits += 8;
        assert(bits <= bitsize);
        if (bits == bitsize) {
            if (need_comma)
                fprintf(f_output, ", ");
            if (need_newline)
                fprintf(f_output, "\n\t");
            if (bitsize == 8)
                fprintf(f_output, "0x%02x", word);
            else if (bitsize == 16)
                fprintf(f_output, "0x%04x", word);
            else if (bitsize == 32)
                fprintf(f_output, "0x%08x", word);
            word = bits = 0;
            need_comma = true;  /* after first value, comma must always precede any next value */
            need_newline = (((idx + 1) & 0x0f) == 0);   /* 16 bytes per row */
        }
    }
    if (bits > 0) {
        if (need_comma)
            fprintf(f_output, ", ");
        if (need_newline)
            fprintf(f_output, "\n\t");
        if (bitsize == 8)
            fprintf(f_output, "0x%02x", word);
        else if (bitsize == 16)
            fprintf(f_output, "0x%04x", word);
        else if (bitsize == 32)
            fprintf(f_output, "0x%08x", word);
    }
    fprintf(f_output, "\n};\n\n");
    if (use_macro)
        fprintf(f_output, "#define %s_size %u\n", symbolname, array_size);
    else
        fprintf(f_output, "const unsigned int %s_size = %u;\n", symbolname, array_size);

#ifdef USE_BZ2
    if (use_macro)
        fprintf(f_output, "#define %s_size_uncompressed %u\n", symbolname, uncompressed_size);
    else
        fprintf(f_output, "const unsigned int %s_size_uncompressed = %u;\n", symbolname, uncompressed_size);
#endif

    fclose(f_output);
    free(buf);
    if (local_outputname)
        free(f_outputname);
    if (local_symbolname)
        free(symbolname);

    return 0;
}
