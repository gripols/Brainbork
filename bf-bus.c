#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* translate(char c) {
    switch (c) {
                case '+': return "ROUTE";
                case '-': return "102";
                case '>': return "MARKHAM";
                case '<': return "ROAD";
                case '[': return "SOUTHBOUND";
                case ']': return "TOWARDS";
                case ',': return "WARDEN";
                case '.': return "STATION";
                default:  return NULL;
        }
}

void create_output_filename(const char *input, char *output, size_t size) {
        const char *dot = strrchr(input, '.');
        if (dot && strcmp(dot, ".bf") == 0)
                snprintf(output, size, "%.*s.bus", (int)(dot - input), input);
        else
                snprintf(output, size, "%s.bus", input);
}

int main(int argc, char *argv[]) {
        if (argc != 2) {
                fprintf(stderr, "Usage: %s <input.bf>\n", argv[0]);
                return EXIT_FAILURE;
        }

        char out_filename[512];
        create_output_filename(argv[1], out_filename, sizeof(out_filename));

        FILE *in = fopen(argv[1], "r");
        if (!in) {
                perror("Error opening input file");
                return EXIT_FAILURE;
        }

        FILE *out = fopen(out_filename, "w");
        if (!out) {
                perror("Error creating output file");
                fclose(in);
                return EXIT_FAILURE;
        }

        int ch;
        while ((ch = fgetc(in)) != EOF) {
                const char *word = translate((char)ch);
                if (word)
                        fprintf(out, "%s ", word);
                else if (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r')
                        fputc(ch, out);
        }

        fclose(in);
        fclose(out);

        printf("Output written to: %s\n", out_filename);
        return EXIT_SUCCESS;
}