#include "brainfork.h"
#include "compiler.h"
#include "vm.h"
#include "jit.h"

static void usage()
{
	printf("usage:\n"
	       "  ./brainbork [options] <filename.bf>\n\n"
	       "options:\n"
	       "  -i | interpreter mode\n"
	       "  -j | JIT mode\n");
}

static char *read_file_to_string(const char *filename)
{
	FILE *file = fopen(filename, "rb");
	if (!file) {
		return NULL;
	}
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	if (file_size < 0) {
		fclose(file);
		return NULL;
	}
	rewind(file);
	char *buffer = (char *)malloc(file_size + 1);
	if (!buffer) {
		fclose(file);
		return NULL;
	}
	size_t bytes_read = fread(buffer, 1, file_size, file);
	if (bytes_read != (size_t)file_size) {
		fclose(file);
		free(buffer);
		return NULL;
	}
	buffer[file_size] = '\0';
	fclose(file);
	return buffer;
}

int main(int argc, const char *argv[])
{
	if (argc == 1) {
		usage();
		return 0;
	}

	int interpreter_mode = 0;
	int jit_compiler_mode = 0;
	int filename_index = -1;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-i") == 0) {
			interpreter_mode = 1;
		} else if (strcmp(argv[i], "-j") == 0) {
			jit_compiler_mode = 1;
		} else if (argv[i][0] != '-') {
			if (filename_index != -1) {
				fprintf(stderr,
					"error: multiple filenames provided.\n\n");
				usage();
				return -1;
			}
			filename_index = i;
		} else {
			fprintf(stderr, "error argument \"%s\"\n\n", argv[i]);
			usage();
			return -1;
		}
	}

	if (!interpreter_mode && !jit_compiler_mode) {
		fprintf(stderr,
			"please choose an interpreter or JIT-compiler\n\n");
		usage();
		return -1;
	}

	if (filename_index < 0) {
		fprintf(stderr, "no input file\n\n");
		usage();
		return -1;
	}

	char *file_content = read_file_to_string(argv[filename_index]);
	if (!file_content) {
		fprintf(stderr, "cannot open or read file <%s>\n",
			argv[filename_index]);
		return -1;
	}

	OpcodeVector code, optimized_code;
	OpcodeVector_init(&code);
	OpcodeVector_init(&optimized_code);

	if (!scanner(file_content, &code)) {
		fprintf(stderr, "Failed to parse the file.\n");
		free(file_content);
		return -1;
	}
	free(file_content);
	file_content = NULL;

	if (!optimize(&code, &optimized_code)) {
		fprintf(stderr, "Failed to optimize the code.\n");
		OpcodeVector_free(&code);
		return -1;
	}

	OpcodeVector_free(&code); // We only need the optimized version now

	if (interpreter_mode) {
		interpreter(&optimized_code);
	}

	if (jit_compiler_mode) {
		if (!jit(&optimized_code)) {
			fprintf(stderr,
				"JIT compilation or execution failed.\n");
			OpcodeVector_free(&optimized_code);
			return -1;
		}
	}

	OpcodeVector_free(&optimized_code);
	return 0;
}
