#include "compiler.h"
#include "util.h"

bool scanner(const char *s, OpcodeVector *out_code)
{
	OpcodeVector code;
	SizeTStack stk;
	OpcodeVector_init(&code);
	SizeTStack_init(&stk);

	uint32_t cnt = 0;
	int line = 1;
	size_t len = strlen(s);

	for (size_t i = 0; i < len; /* increment inside loop */) {
		char cmd = s[i];

		if (strchr("+-<>[].,", cmd) == NULL) {
			if (cmd == '\n')
				++line;
			++i;
			continue;
		}

		cnt = 0;
		if (strchr("+-<>", cmd)) {
			while (i < len && s[i] == cmd) {
				++cnt;
				++i;
			}
		} else {
			cnt = 1;
			++i;
		}

		opcode op = { 0, 0 };
		switch (cmd) {
		case '+':
			op = (opcode){ op_add, cnt };
			break;
		case '-':
			op = (opcode){ op_sub, cnt };
			break;
		case '>':
			op = (opcode){ op_addp, cnt };
			break;
		case '<':
			op = (opcode){ op_subp, cnt };
			break;
		case '[':
			if (!SizeTStack_push(&stk, code.size))
				goto error;
			op = (opcode){ op_jf, 0 }; // Placeholder target
			break;
		case ']': {
			size_t jf_idx;
			if (!SizeTStack_top(&stk, &jf_idx)) {
				fprintf(stderr,
					"Error: Mismatched ']' at line %d\n",
					line);
				goto error;
			}
			SizeTStack_pop(&stk);

			op = (opcode){ op_jt,
				       (uint32_t)jf_idx }; // Jump back to '['
			code.data[jf_idx].num =
				(uint32_t)(code.size +
					   1); // Patch '[' to jump *after* ']'
			break;
		}
		case ',':
			op = (opcode){ op_in, 0 };
			for (uint32_t j = 1; j < cnt; ++j) {
				if (!OpcodeVector_push_back(&code, op))
					goto error;
			}
			break;
		case '.':
			op = (opcode){ op_out, 0 };
			for (uint32_t j = 1; j < cnt; ++j) {
				if (!OpcodeVector_push_back(&code, op))
					goto error;
			}
			break;
		}

		if (!OpcodeVector_push_back(&code, op))
			goto error;
	}

	if (!SizeTStack_empty(&stk)) {
		fprintf(stderr, "Error: Mismatched '[' at end of file.\n");
		goto error;
	}

	SizeTStack_free(&stk);
	*out_code = code;
	return true;

error:
	SizeTStack_free(&stk);
	OpcodeVector_free(&code);
	return false;
}

bool optimize(const OpcodeVector *in_code, OpcodeVector *out_code)
{
	OpcodeVector_init(out_code);

	const size_t map_size = in_code->size + 1;
	size_t *old_to_new_map = malloc(map_size * sizeof(size_t));
	if (!old_to_new_map) {
		perror("Failed to allocate optimizer map");
		return false;
	}

	const size_t NOT_MAPPED = (size_t)-1;
	for (size_t i = 0; i < map_size; ++i) {
		old_to_new_map[i] = NOT_MAPPED;
	}

	for (size_t i = 0; i < in_code->size; ++i) {
		const opcode *op = &in_code->data[i];

		old_to_new_map[i] = out_code->size;

		if (op->op == op_jf && (i + 2 < in_code->size) &&
		    (in_code->data[i + 1].op == op_sub ||
		     in_code->data[i + 1].op == op_add) &&
		    (in_code->data[i + 1].num == 1) &&
		    (in_code->data[i + 2].op == op_jt) &&
		    (in_code->data[i + 2].num == i) && // `]` jumps to `[`
		    (op->num == (i + 3)) // `[` jumps past `]`
		) {
			opcode clear_op = { op_clear, 0 };
			if (!OpcodeVector_push_back(out_code, clear_op)) {
				free(old_to_new_map);
				return false;
			}

			old_to_new_map[i + 1] = NOT_MAPPED;
			old_to_new_map[i + 2] = NOT_MAPPED;

			i += 2; // Skip all 3 opcodes
			continue;
		}

		if (!OpcodeVector_push_back(out_code, *op)) {
			free(old_to_new_map);
			return false;
		}
	}

	old_to_new_map[in_code->size] = out_code->size;

	for (size_t i = 0; i < out_code->size; ++i) {
		opcode *op = &out_code->data[i];
		if (op->op == op_jf || op->op == op_jt) {
			uint32_t old_target_index = op->num;

			if (old_target_index >= map_size) {
				fprintf(stderr,
					"Optimizer: Found jump to invalid old index %u\n",
					old_target_index);
				free(old_to_new_map);
				return false;
			}

			size_t new_target_index =
				old_to_new_map[old_target_index];

			if (new_target_index == NOT_MAPPED) {
				fprintf(stderr,
					"Optimizer: Jump target %u was optimized away!\n",
					old_target_index);
				free(old_to_new_map);
				return false;
			}

			op->num = (uint32_t)new_target_index;
		}
	}

	free(old_to_new_map);
	return true;
}
