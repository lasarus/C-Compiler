#include "parser.h"
#include "declaration.h"
#include "symbols.h"

#include <common.h>
#include <preprocessor/preprocessor.h>

#include <stdlib.h>
#include <string.h>

block_id new_block() {
	static int block_counter = 1; // reserve space for null block.
	return block_counter++;
}

void parse_into_ir() {
	init_variables();

	while (parse_declaration(1));
	if (TACCEPT(T_SEMI_COLON)) {
		PRINT_POS(T0->pos);
		ERROR("Extra semicolon outside function.");
	}
	TEXPECT(T_EOI);
}

struct program program;

void push_ir(struct instruction instruction) {
	if (program.size >= program.capacity) {
		program.capacity = MAX(program.capacity * 2, 2);
		program.instructions = realloc(program.instructions,
									   sizeof (*program.instructions) * program.capacity);
	}
	program.instructions[program.size++] = instruction;
}

struct program *get_program(void) {
	return &program;
}

const char *instruction_to_str(struct instruction ins) {
	static int char_buffer_size = 100;
	static char *buffer = NULL;

	if (!buffer) {
		buffer = malloc(char_buffer_size);
	}

	int curr_pos = 0;
	
#define PRINT(STR, ...) do {											\
		int print_size = snprintf(buffer + curr_pos, char_buffer_size - 1 - curr_pos, STR, ##__VA_ARGS__); \
		int req_size = print_size + 1 + curr_pos; \
		if (req_size > char_buffer_size) {								\
			char_buffer_size = req_size;								\
			buffer = realloc(buffer, char_buffer_size);					\
			snprintf(buffer + curr_pos, char_buffer_size - 1 - curr_pos, STR, ##__VA_ARGS__); \
		}																\
		curr_pos += print_size;\
	} while (0)

	switch (ins.type) {
	case IR_FUNCTION:
		PRINT("START OF FUNCTION: %s \"", ins.function.name);
		PRINT("\" with named arguments: ");
		for (int i = 0; i < ins.function.signature->n - 1; i++) {
			PRINT("%d ", ins.function.named_arguments[i]);
		}
		break;

	case IR_CONSTANT:
		PRINT("%d = constant", ins.constant.result);
		break;

	case IR_BINARY_OPERATOR:
		PRINT("%d = binary_op(%d, %d)", ins.binary_operator.result,
			   ins.binary_operator.lhs,
			   ins.binary_operator.rhs);
		break;

	case IR_UNARY_OPERATOR:
		PRINT("%d = unary_op(%d)", ins.unary_operator.result,
			   ins.unary_operator.operand);
		break;

	case IR_RETURN:
		PRINT("return %d", ins.return_.value);
		break;

	case IR_ALLOCA:
		PRINT("allocate space for %d", ins.alloca.variable);
		break;

	case IR_CALL_VARIABLE:
		PRINT("%d = %d (", ins.call_variable.result, ins.call_variable.function);
		for (int i = 0; i < ins.call_variable.n_args; i++) {
			if (i)
				PRINT(", ");
			PRINT("%d", ins.call_variable.args[i]);
		}
		PRINT(")");
		break;

	case IR_LOAD:
		PRINT("%d = load %d", ins.load.result, ins.load.pointer);
		break;

	case IR_STORE:
		PRINT("store %d into %d", ins.store.value, ins.store.pointer);
		break;

	case IR_POINTER_INCREMENT:
		PRINT("%d = increment %d by %d", ins.pointer_increment.result, ins.pointer_increment.pointer, ins.pointer_increment.index);
		break;

	case IR_COPY: {
		PRINT("%d = %d", ins.copy.result, ins.copy.source);
	} break;

	case IR_START_BLOCK:
		PRINT("BLOCK %d:", ins.start_block.block);
		break;

	case IR_IF_SELECTION:
		PRINT(" IF %d {goto %d} else {goto %d}:", ins.if_selection.condition,
			   ins.if_selection.block_true, ins.if_selection.block_false);
		break;

	case IR_SWITCH_SELECTION:
		PRINT(" SWITCH ON %d:", ins.switch_selection.condition);
		break;

	case IR_GOTO:
		PRINT("goto %d", ins.goto_.block);
		break;

	case IR_CAST:
		PRINT("%d (%s)", ins.cast.result, type_to_string(ins.cast.result_type));
		PRINT(" = cast %d (%s)", ins.cast.rhs, type_to_string(ins.cast.rhs_type));
		break;

	case IR_ADDRESS_OF:
		PRINT("%d = address of %d", ins.address_of.result, ins.address_of.variable);
		break;

	case IR_GET_MEMBER:
		PRINT("%d = get offset %d of %d", ins.get_member.result,
			   ins.get_member.offset,
			   ins.get_member.pointer);
		break;

	case IR_VA_ARG:
		PRINT("%d = v_arg", ins.va_arg_.result);
		break;

	case IR_VA_START:
		PRINT("%d = v_start", ins.va_start_.result);
		break;

	case IR_SET_ZERO:
		PRINT("%d = zero", ins.set_zero.variable);
		break;

	case IR_ASSIGN_CONSTANT_OFFSET:
		PRINT(" %d [at offset %d] = %d",
			   ins.assign_constant_offset.variable,
			   ins.assign_constant_offset.offset,
			   ins.assign_constant_offset.value
			);
		break;

	case IR_STACK_ALLOC:
		PRINT("%d = allocate %d on stack", ins.stack_alloc.pointer,
			  ins.stack_alloc.length);
		break;

	case IR_POP_STACK_ALLOC:
		PRINT("pop stack allocation");
		break;

	case IR_GET_SYMBOL_PTR:
		PRINT("%d = get symbol %s", ins.get_symbol_ptr.result,
			  ins.get_symbol_ptr.label);
		break;

	case IR_STATIC_VAR:
		PRINT("%s = static alloc", ins.static_var.label);
		break;

	case IR_POINTER_DIFF:
		PRINT("%d = pointer_diff(%d - %d)", ins.pointer_diff.result,
			  ins.pointer_diff.lhs,
			  ins.pointer_diff.rhs);
		break;

	default:
		printf("%d", ins.type);
		NOTIMP();
	}

	return buffer;
}

void print_instruction(struct instruction instruction) {
	const char *str = instruction_to_str(instruction);
	printf("\t%s\n", str);
}

void print_parser_ir() {
	for (int i = 0; i < program.size; i++) {
		print_instruction(program.instructions[i]);
	}
}

enum operand_type ot_from_st(enum simple_type st) {
	switch (st) {
	case ST_INT: return OT_INT;
	case ST_UINT: return OT_UINT;
	case ST_LONG: return OT_LONG;
	case ST_ULONG: return OT_ULONG;
	case ST_LLONG: return OT_LLONG;
	case ST_ULLONG: return OT_ULLONG;
	default: ERROR("Invalid operand type %d", st);
	}
}

enum operand_type ot_from_type(struct type *type) {
	if (type_is_pointer(type)) {
		return OT_PTR;
	} else if (type->type == TY_SIMPLE) {
		return ot_from_st(type->simple);
	} else {
		ERROR("Invalid operand type");
	}
}
