#include "debug.h"

#include <common.h>

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
			buffer = realloc(buffer, char_buffer_size);					\
			snprintf(buffer + curr_pos, char_buffer_size - 1 - curr_pos, STR, ##__VA_ARGS__); \
		}																\
		curr_pos += print_size;\
	} while (0)

const char *dbg_type(struct type *type) {
	static int char_buffer_size = 100;
	static char *buffer = NULL;

	if (!buffer) {
		buffer = malloc(char_buffer_size);
	}

	int curr_pos = 0;

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
		buffer = malloc(char_buffer_size);
	}

	int curr_pos = 0;

	switch (ins.type) {
	case IR_CONSTANT:
		DBG_PRINT("%d = constant", ins.result);
		break;

	case IR_BINARY_OPERATOR:
		DBG_PRINT("%d = binary_op(%d, %d) of type %d", ins.result,
				  ins.binary_operator.lhs,
				  ins.binary_operator.rhs,
				  ins.binary_operator.type);
		break;

	case IR_UNARY_OPERATOR:
		DBG_PRINT("%d = unary_op(%d)", ins.result,
			   ins.unary_operator.operand);
		break;

	case IR_CALL_VARIABLE:
		DBG_PRINT("%d = %d (", ins.result, ins.call_variable.function);
		for (int i = 0; i < ins.call_variable.n_args; i++) {
			if (i)
				DBG_PRINT(", ");
			DBG_PRINT("%d", ins.call_variable.args[i]);
		}
		DBG_PRINT(")");
		break;

	case IR_LOAD:
		DBG_PRINT("%d = load %d", ins.result, ins.load.pointer);
		break;

	case IR_STORE:
		DBG_PRINT("store %d into %d", ins.store.value, ins.store.pointer);
		break;

	case IR_COPY:
		DBG_PRINT("%d = %d", ins.result, ins.copy.source);
		break;

	case IR_CAST:
		DBG_PRINT("%d (%s)", ins.result, dbg_type(ins.cast.result_type));
		DBG_PRINT(" = cast %d (%s)", ins.cast.rhs, dbg_type(ins.cast.rhs_type));
		break;

	case IR_ADDRESS_OF:
		DBG_PRINT("%d = address of %d", ins.result, ins.address_of.variable);
		break;

	case IR_VA_ARG:
		DBG_PRINT("%d = v_arg", ins.result);
		break;

	case IR_VA_START:
		DBG_PRINT("%d = v_start", ins.result);
		break;

	case IR_SET_ZERO:
		DBG_PRINT("%d = zero", ins.result);
		break;

	case IR_STACK_ALLOC:
		DBG_PRINT("%d = allocate %d on stack", ins.result,
			  ins.stack_alloc.length);
		break;

	case IR_CLEAR_STACK_BUCKET:
		DBG_PRINT("clear stack bucket %d", ins.clear_stack_bucket.stack_bucket);
		break;

	case IR_ADD_TEMPORARY:
		DBG_PRINT("%d <- temporary", ins.result);
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
		buffer = malloc(char_buffer_size);
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
