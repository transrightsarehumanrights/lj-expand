/* LJE: Very quick build utility to convert Lua scripts to C byte arrays. */

#include <stdio.h>
#include <stdlib.h>

void print_usage(const char* prog_name) {
    printf("Usage: %s <input.lua> <output.h> <name>\n", prog_name);
    printf("Converts a Lua script to a C byte array.\n");
    printf("The name parameter specifies the name of the generated array.\n");
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char* input_path = argv[1];
    const char* output_path = argv[2];
    const char* array_name = argv[3];

    FILE* input_file = fopen(input_path, "rb");
    if (!input_file) {
        perror("Failed to open input file");
        return 1;
    }

    FILE* output_file = fopen(output_path, "w");
    if (!output_file) {
        perror("Failed to open output file");
        fclose(input_file);
        return 1;
    }

    fprintf(output_file, "unsigned char %s_data[] = {\n", array_name);

    int byte;
    int count = 0;
    while ((byte = fgetc(input_file)) != EOF) {
        if (count % 12 == 0) {
            fprintf(output_file, "    ");
        }
        fprintf(output_file, "0x%02x,", (unsigned char)byte);
        count++;
        if (count % 12 == 0) {
            fprintf(output_file, "\n");
        }
    }

    if (count % 12 == 0) {
        fprintf(output_file, "    ");
    }

    fprintf(output_file, "0x00");
    count++;

    if (count % 12 != 0) {
        fprintf(output_file, "\n");
    }

    fprintf(output_file, "};\n");
    fprintf(output_file, "unsigned int %s_len = %d;\n", array_name, count);

    fclose(input_file);
    fclose(output_file);

    printf("Successfully converted '%s' to '%s'\n", input_path, output_path);
    return 0;
}