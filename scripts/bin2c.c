#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s <input_file> <prefix> <output_file>", argv[0]);
        return -1;
    }

    FILE *f_in = fopen(argv[1], "rb");
    FILE *f_out = fopen(argv[3], "w");
    if (!f_in)
    {
        printf("Couldn't open %s", argv[1]);
        return 1;
    }
    if (!f_out)
    {
        printf("Couldn't open %s", argv[3]);
        return 3;
    }

    uint64_t file_size = 0;
    fseek(f_in, 0, SEEK_END);
    file_size = ftell(f_in);
    fseek(f_in, 0, SEEK_SET);

    fprintf(f_out,
            "static const unsigned int %s_size = %zd;\n"
            "static const unsigned char %s_data[%zd] = {\n",
            argv[2], file_size, argv[2], file_size);

    for (uint64_t idx = 0; idx < file_size; idx++)
    {
        fprintf(f_out, "%u,", (uint8_t)fgetc(f_in));
    }

    fprintf(f_out, "\n};");

    fclose(f_in);
    fclose(f_out);

    return 0;
}
