#include "debug.h"

#include <common.h>
#include <codegen/registers.h>

#include <stdlib.h>

static const char *simple_to_str(enum simple_type st) {
	switch (st) {
	case ST_VOID: return "ST_VOID";
	case ST_CHAR: return "ST_CHAR";
	case ST_SCHAR: return "ST_SCHAR";
	case ST_UCHAR: return "ST_UCHAR";
	case ST_SHORT: return "ST_SHORT";
	case ST_USHORT: return "ST_USHORT";
	case ST_INT: return "ST_INT";
	case ST_UINT: return "ST_UINT";
	case ST_LONG: return "ST_LONG";
	case ST_ULONG: return "ST_ULONG";
	case ST_LLONG: return "ST_LLONG";
	case ST_ULLONG: return "ST_ULLONG";
	case ST_FLOAT: return "ST_FLOAT";
	case ST_DOUBLE: return "ST_DOUBLE";
	case ST_LDOUBLE: return "ST_LDOUBLE";
	case ST_BOOL: return "ST_BOOL";
	case ST_FLOAT_COMPLEX: return "ST_FLOAT_COMPLEX";
	case ST_DOUBLE_COMPLEX: return "ST_DOUBLE_COMPLEX";
	case ST_LDOUBLE_COMPLEX: return "ST_LDOUBLE_COMPLEX";
	default: return "???";
	}
}
	
#define DBG_PRINT(...) do {											\
		int print_size = snprintf(buffer + curr_pos, char_buffer_size - 1 - curr_pos, __VA_ARGS__); \
		int req_size = print_size + 1 + curr_pos; \
		if (req_size > char_buffer_size) {								\
			char_buffer_size = req_size;								\
			buffer = cc_realloc(buffer, char_buffer_size);					\
			snprintf(buffer + curr_pos, char_buffer_size - 1 - curr_pos, __VA_ARGS__); \
		}																\
		curr_pos += print_size;\
	} while (0)

const char *dbg_type(struct type *type) {
	static int char_buffer_size = 100;
	static char *buffer = NULL;

	if (!buffer) {
		buffer = cc_malloc(char_buffer_size);
	}

	int curr_pos = 0;

	if (type->is_const)
		DBG_PRINT("CONST ");

	while (type) {
		switch (type->type) {
		case TY_SIMPLE:
			DBG_PRINT("SIMPLE %s", simple_to_str(type->simple));
			type = NULL;
			break;
		case TY_ARRAY:
			DBG_PRINT("ARRAY of ");
			type = type->children[0];
			break;
		case TY_VARIABLE_LENGTH_ARRAY:
			DBG_PRINT("VARIABLE LENGTH ARRAY OF ");
			type = type->children[0];
			break;
		case TY_POINTER:
			DBG_PRINT("POINTER to ");
			type = type->children[0];
			break;
		case TY_FUNCTION:
			DBG_PRINT("FUNCTION returning ");
			type = type->children[0];
			break;
		case TY_STRUCT:
			DBG_PRINT("STRUCT (%.*s)", type->struct_data->name.len, type->struct_data->name.str);
			type = NULL;
			break;
		case TY_INCOMPLETE_ARRAY:
			DBG_PRINT("INCOMPLETE ARRAY of ");
			type = type->children[0];
			break;
		default:
			type = NULL;
		}
	}
	return buffer;
}

const char *dbg_instruction(struct instruction *ins) {
	static int char_buffer_size = 100;
	static char *buffer = NULL;

	if (!buffer) {
		buffer = cc_malloc(char_buffer_size);
	}

	int curr_pos = 0;

	switch (ins->type) {
	case IR_CONSTANT:
		DBG_PRINT("%d = constant", ins->result);
		break;

	case IR_BINARY_NOT:
		DBG_PRINT("%d = bnot(%d)", ins->result, ins->arguments[0]);
		break;

	case IR_NEGATE_INT:
		DBG_PRINT("%d = negate int(%d)", ins->result, ins->arguments[0]);
		break;

	case IR_NEGATE_FLOAT:
		DBG_PRINT("%d = negate float(%d)", ins->result, ins->arguments[0]);
		break;

	case IR_CALL:
		DBG_PRINT("call %d ( ... args ... )", ins->result);
		break;

	case IR_LOAD:
		DBG_PRINT("%d = load %d", ins->result, ins->arguments[0]);
		break;

	case IR_LOAD_BASE_RELATIVE:
		DBG_PRINT("%d = load %d relative to base", ins->result, ins->load_base_relative.offset);
		break;

	case IR_LOAD_BASE_RELATIVE_ADDRESS:
		DBG_PRINT("*%d = load %d relative to base", ins->result, ins->load_base_relative.offset);
		break;

	case IR_STORE:
		DBG_PRINT("store %d into %d", ins->result, ins->arguments[0]);
		break;

	case IR_STORE_STACK_RELATIVE:
		DBG_PRINT("store %d into %d relative to stack", ins->result, ins->store_stack_relative.offset);
		break;

	case IR_INT_CAST_ZERO:
		DBG_PRINT("%d = int_cast_zero %d", ins->result, ins->arguments[0]);
		break;

	case IR_INT_CAST_SIGN:
		DBG_PRINT("%d = int_cast_zero %d", ins->result, ins->arguments[0]);
		break;

	case IR_BOOL_CAST:
		DBG_PRINT("%d = bool_cast %d", ins->result, ins->arguments[0]);
		break;

	case IR_FLOAT_CAST:
		DBG_PRINT("%d = float_cast %d", ins->result, ins->arguments[0]);
		break;

	case IR_INT_FLOAT_CAST:
		DBG_PRINT("%d = int_float_cast %d", ins->result, ins->arguments[0]);
		break;

	case IR_UINT_FLOAT_CAST:
		DBG_PRINT("%d = uint_float_cast %d", ins->result, ins->arguments[0]);
		break;

	case IR_FLOAT_INT_CAST:
		DBG_PRINT("%d = float_int_cast %d", ins->result, ins->arguments[0]);
		break;

	case IR_VA_ARG:
		DBG_PRINT("%d = v_arg", ins->result);
		break;

	case IR_VA_START:
		DBG_PRINT("%d = v_start", ins->result);
		break;

	case IR_SET_ZERO_PTR:
		DBG_PRINT("zero ptr %d, size: %d", ins->result, ins->set_zero_ptr.size);
		break;

	case IR_VLA_ALLOC:
		DBG_PRINT("%d = allocate vla of length %d", ins->result,
				  ins->arguments[0]);
		break;

	case IR_GET_REG:
		DBG_PRINT("%d <- %s", ins->result, get_reg_name(ins->get_reg.register_index, 8));
		break;

	case IR_SET_REG:
		DBG_PRINT("set_reg %s %d", get_reg_name(ins->set_reg.register_index, 8), ins->result);
		break;

	case IR_MODIFY_STACK_POINTER:
		DBG_PRINT("modify stack pointer by %d", ins->modify_stack_pointer.change);
		break;

	case IR_LOAD_PART_ADDRESS:
		DBG_PRINT("%d = load part address of %d with offset %d", ins->result, ins->arguments[0], ins->load_part.offset);
		break;

	case IR_STORE_PART_ADDRESS:
		DBG_PRINT("*%d = store part of %d with offset %d", ins->result, ins->arguments[0], ins->load_part.offset);
		break;

	case IR_ALLOC:
		DBG_PRINT("%d = alloc with size %d (save to preamble: %d)", ins->result, ins->alloc.size, ins->alloc.save_to_preamble);
		break;

	default:
		DBG_PRINT("%d <- %d op(%d) %d", ins->result, ins->arguments[0],
				  ins->type, ins->arguments[1]);
		break;

	/* default: */
	/* 	printf("%d", ins.type); */
	/* 	NOTIMP(); */
	}

	return buffer;
}

const char *dbg_token(struct token *t) {
	static int char_buffer_size = 100;
	static char *buffer = NULL;

	if (!buffer) {
		buffer = cc_malloc(char_buffer_size);
	}

	int curr_pos = 0;

	if (t->type == T_IDENT) {
		DBG_PRINT("%.*s", t->str.len, t->str.str);
	} else if (t->type == T_NUM) {
		DBG_PRINT("%.*s", t->str.len, t->str.str);
	} else if (t->type == T_STRING) {
		DBG_PRINT("\"%.*s\"", t->str.len, t->str.str);
	} else {
		DBG_PRINT("%s", dbg_token_type(t->type));
	}

	return buffer;
}

const char *dbg_token_type(enum ttype tt) {
	switch(tt) {
#define PRINT(A, B) case A: return B;
#define X(A, B) PRINT(A, B)
#define SYM(A, B) PRINT(A, B)
#define KEY(A, B) PRINT(A, B)
#include "preprocessor/tokens.h"
#undef KEY
#undef X
#undef SYM
#undef PRINT
	default:
		return "";
	}
}
