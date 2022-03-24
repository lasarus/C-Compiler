#include "debug.h"

#include <common.h>
#include <codegen/registers.h>

#include <stdlib.h>

const char *simple_to_str(enum simple_type st) {
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
	
#define DBG_PRINT(STR, ...) do {											\
		int print_size = snprintf(buffer + curr_pos, char_buffer_size - 1 - curr_pos, STR, ##__VA_ARGS__); \
		int req_size = print_size + 1 + curr_pos; \
		if (req_size > char_buffer_size) {								\
			char_buffer_size = req_size;								\
			buffer = cc_realloc(buffer, char_buffer_size);					\
			snprintf(buffer + curr_pos, char_buffer_size - 1 - curr_pos, STR, ##__VA_ARGS__); \
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

const char *dbg_instruction(struct instruction ins) {
	static int char_buffer_size = 100;
	static char *buffer = NULL;

	if (!buffer) {
		buffer = cc_malloc(char_buffer_size);
	}

	int curr_pos = 0;

	switch (ins.type) {
	case IR_CONSTANT:
		DBG_PRINT("%d = constant", ins.operands[0]);
		break;

	case IR_BINARY_OPERATOR:
		DBG_PRINT("%d = binary_op(%d, %d) of type %d", ins.operands[0],
				  ins.operands[1], ins.operands[2],
				  ins.binary_operator.type);
		break;

	case IR_BINARY_NOT:
		DBG_PRINT("%d = bnot(%d)", ins.operands[0], ins.operands[1]);
		break;

	case IR_NEGATE_INT:
		DBG_PRINT("%d = negate int(%d)", ins.operands[0], ins.operands[1]);
		break;

	case IR_NEGATE_FLOAT:
		DBG_PRINT("%d = negate float(%d)", ins.operands[0], ins.operands[1]);
		break;

	case IR_CALL:
		DBG_PRINT("call %d ( ... args ... )", ins.operands[0]);
		break;

	case IR_LOAD:
		DBG_PRINT("%d = load %d", ins.operands[0], ins.operands[1]);
		break;

	case IR_LOAD_BASE_RELATIVE:
		DBG_PRINT("%d = load %d relative to base", ins.operands[0], ins.load_base_relative.offset);
		break;

	case IR_STORE:
		DBG_PRINT("store %d into %d", ins.operands[0], ins.operands[1]);
		break;

	case IR_STORE_STACK_RELATIVE:
		DBG_PRINT("store %d into %d relative to stack", ins.operands[0], ins.store_stack_relative.offset);
		break;

	case IR_COPY:
		DBG_PRINT("%d = %d", ins.operands[0], ins.operands[1]);
		break;

	case IR_INT_CAST:
		DBG_PRINT("%d = int_cast %d (%s)", ins.operands[0], ins.operands[1], ins.int_cast.sign_extend ? "signed" : "not signed");
		break;

	case IR_BOOL_CAST:
		DBG_PRINT("%d = bool_cast %d", ins.operands[0], ins.operands[1]);
		break;

	case IR_FLOAT_CAST:
		DBG_PRINT("%d = float_cast %d", ins.operands[0], ins.operands[1]);
		break;

	case IR_INT_FLOAT_CAST:
		DBG_PRINT("%d = float_cast %d, from float: %d, sign: %d", ins.operands[0], ins.operands[1],
				  ins.int_float_cast.from_float, ins.int_float_cast.sign);
		break;

	case IR_ADDRESS_OF:
		DBG_PRINT("%d = address of %d", ins.operands[0], ins.operands[1]);
		break;

	case IR_VA_ARG:
		DBG_PRINT("%d = v_arg", ins.operands[0]);
		break;

	case IR_VA_START:
		DBG_PRINT("%d = v_start", ins.operands[0]);
		break;

	case IR_SET_ZERO:
		DBG_PRINT("%d = zero", ins.operands[0]);
		break;

	case IR_STACK_ALLOC:
		DBG_PRINT("%d = allocate %d on stack", ins.operands[0],
			  ins.operands[1]);
		break;

	case IR_CLEAR_STACK_BUCKET:
		DBG_PRINT("clear stack bucket %d", ins.clear_stack_bucket.stack_bucket);
		break;

	case IR_ADD_TEMPORARY:
		DBG_PRINT("%d <- temporary", ins.operands[0]);
		break;

	case IR_GET_REG:
		DBG_PRINT("%d <- %s", ins.operands[0], get_reg_name(ins.get_reg.register_index, 8));
		break;

	case IR_SET_REG:
		DBG_PRINT("set_reg %s %d", get_reg_name(ins.set_reg.register_index, 8), ins.operands[0]);
		break;

	case IR_MODIFY_STACK_POINTER:
		DBG_PRINT("modify stack pointer by %d", ins.modify_stack_pointer.change);
		break;

	default:
		printf("%d", ins.type);
		NOTIMP();
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
