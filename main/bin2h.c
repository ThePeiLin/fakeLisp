#include <argtable3.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define COLUMN (12)

struct arg_lit *help;
struct arg_file *input;
struct arg_file *output;
struct arg_end *end;

int main(int argc, char **argv) {
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    int exitcode = 0;
    int nerrors = 0;
    uint8_t *data = NULL;
    const char *progname = argv[0];

    void *argtable[] = {
        help = arg_lit0("h", "help", "display this help and exit"),
        input = arg_file1("i", NULL, "<file-name>", "input file name"),
        output = arg_file1("o", NULL, "<file-name>", "output file name"),
        end = arg_end(20),
    };

    nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        printf("Usage: %s", progname);
        arg_print_syntaxv(stdout, argtable, "\n");
        printf("compile a file into bytecode or pre-compile a package.\n\n");
        arg_print_glossary_gnu(stdout, argtable);
        goto exit;
    }

    if (nerrors) {
        arg_print_errors(stdout, end, progname);
        printf("Try '%s --help' for more informaction.\n", progname);
        exitcode = EXIT_FAILURE;
        goto exit;
    }

    input_file = fopen(input->filename[0], "rb");
    if (input_file == NULL) {
        perror(input->filename[0]);
        exitcode = EXIT_FAILURE;
        goto exit;
    }

    output_file = fopen(output->filename[0], "w");
    if (output_file == NULL) {
        perror(output->filename[0]);
        exitcode = EXIT_FAILURE;
        goto exit;
    }

    size_t len = 0;
    {
        fseek(input_file, 0, SEEK_END);
        long r = ftell(input_file);
        if (r < 0) {
            perror(input->filename[0]);
            exitcode = EXIT_FAILURE;
            goto exit;
        }
        len = (size_t)r;
        fseek(input_file, 0, SEEK_SET);
    }

    data = (uint8_t *)malloc(len * sizeof(uint8_t));
    assert(data);

    if (fread(data, len, 1, input_file) < 1) {
        perror(input->filename[0]);
        exitcode = EXIT_FAILURE;
        goto exit;
    }

    for (size_t i = 0; i < len;) {
        fprintf(output_file, "%#04x, ", data[i]);
        ++i;
        if (i && (i % COLUMN == 0))
            fputc('\n', output_file);
    }

exit:
    if (data)
        free(data);
    if (input_file)
        fclose(input_file);
    if (output_file)
        fclose(output_file);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitcode;
}
