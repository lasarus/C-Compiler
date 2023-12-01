#include "debug.h"
#include "ir/ir.h"

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

const char *dbg_instruction(struct node *ins) {
	static int char_buffer_size = 100;
	static char *buffer = NULL;

	if (!buffer) {
		buffer = cc_malloc(char_buffer_size);
	}

	int curr_pos = 0;

	DBG_PRINT("%d = ", ins->index);

	int n_args = node_argument_count(ins);
	const char *str = NULL;

	switch (ins->type) {
	case IR_BINARY_NOT: str = "~%d"; break;
	case IR_NEGATE_INT: str = "-%d"; break;
	case IR_NEGATE_FLOAT: str = "-%d (float)"; break;
	case IR_LOAD_VOLATILE: str = "load_volatile %d, state: %d"; break;
	case IR_LOAD: str = "load %d, state: %d"; break;
	case IR_STORE: str = "store %d %d, state: %d"; break;
	case IR_INT_CAST_ZERO: str = "int_cast_zero %d"; break;
	case IR_INT_CAST_SIGN: str = "int_cast_zero %d"; break;
	case IR_BOOL_CAST: str = "bool_cast %d"; break;
	case IR_FLOAT_CAST: str = "float_cast %d"; break;
	case IR_INT_FLOAT_CAST: str = "int_float_cast %d"; break;
	case IR_UINT_FLOAT_CAST: str = "uint_float_cast %d"; break;
	case IR_FLOAT_INT_CAST: str = "float_int_cast %d"; break;
	case IR_VA_START: str = "va_start %d"; break;
	case IR_ADD: str = "%d + %d"; break;
	case IR_SUB: str = "%d - %d"; break;
	case IR_MUL: str = "%d * %d"; break;
	case IR_IMUL: str = "%d * %d (signed)"; break;
	case IR_DIV: str = "%d / %d"; break;
	case IR_IDIV: str = "%d / %d (signed)"; break;
	case IR_MOD: str = "%d %% %d"; break;
	case IR_IMOD: str = "%d %% %d (signed)"; break;
	case IR_LSHIFT: str = "%d << %d"; break;
	case IR_RSHIFT: str = "%d >> %d"; break;
	case IR_IRSHIFT: str = "%d >> %d (signed)"; break;
	case IR_BXOR: str = "%d ^ %d"; break;
	case IR_BOR: str = "%d | %d"; break;
	case IR_BAND: str = "%d & %d"; break;
	case IR_LESS: str = "%d < %d"; break;
	case IR_ILESS: str = "%d < %d (signed)"; break;
	case IR_GREATER: str = "%d > %d"; break;
	case IR_IGREATER: str = "%d > %d (signed)"; break;
	case IR_LESS_EQ: str = "%d <= %d"; break;
	case IR_ILESS_EQ: str = "%d <= %d (signed)"; break;
	case IR_GREATER_EQ: str = "%d >= %d"; break;
	case IR_IGREATER_EQ: str = "%d >= %d (signed)"; break;
	case IR_EQUAL: str = "%d == %d"; break;
	case IR_NOT_EQUAL: str = "%d != %d"; break;
	case IR_FLT_ADD: str = "%d + %d (float)"; break;
	case IR_FLT_SUB: str = "%d - %d (float)"; break;
	case IR_FLT_DIV: str = "%d / %d (float)"; break;
	case IR_FLT_MUL: str = "%d * %d (float)"; break;
	case IR_FLT_LESS: str = "%d < %d (float)"; break;
	case IR_FLT_GREATER: str = "%d > %d (float)"; break;
	case IR_FLT_LESS_EQ: str = "%d <= %d (float)"; break;
	case IR_FLT_GREATER_EQ: str = "%d >= %d (float)"; break;
	case IR_FLT_EQUAL: str = "%d == %d (float)"; break;
	case IR_FLT_NOT_EQUAL: str = "%d != %d (float)"; break;
	case IR_IF: str = "if %d %d"; break;
	case IR_ZERO: str = "zero"; break;
	case IR_DEAD: str = "dead"; break;
	case IR_UNDEFINED: str = "undefined"; break;
	case IR_PHI: {
		DBG_PRINT("phi (region: %d)", ins->arguments[0]->index);

		if (ins->arguments[1])
			DBG_PRINT(" %d", ins->arguments[1]->index);
		else
			DBG_PRINT(" (NULL)");

		if (ins->arguments[2])
			DBG_PRINT(" %d", ins->arguments[2]->index);
		else
			DBG_PRINT(" (NULL)");
	} break;
	case IR_REGION:
		if (n_args == 2) {
			str = "region %d %d";
		} else if (n_args == 1) {
			str = "region %d";
		} else {
			str = "region";
		}
		break;
	case IR_VLA_ALLOC: str = "vla_alloc %d"; break;
	case IR_RETURN: str = "return %d, %d, %d"; break;

	case IR_CONSTANT: // TODO: Print the constant.
		DBG_PRINT("constant(%li)", ins->constant.constant.int_d);
		break;

	case IR_CALL: // TODO: Print non_clobbered_register.
		str = "call %d";
		break;

	case IR_VA_ARG: // TODO: Print type.
		str = "v_arg %d %d";
		break;

	case IR_LOAD_BASE_RELATIVE: str = "load %d relative to base"; break;
	case IR_LOAD_BASE_RELATIVE_ADDRESS: str = "load %d relative to base"; break;
	case IR_STORE_STACK_RELATIVE: str = "store %d into %d relative to stack"; break;
	case IR_STORE_STACK_RELATIVE_ADDRESS: str = "store address %d into %d relative to stack"; break;

	case IR_GET_REG:
		DBG_PRINT("%s", get_reg_name(ins->get_reg.register_index, 8));
		break;

	case IR_SET_ZERO_PTR:
		DBG_PRINT("set_zero_ptr %d, size: %d", ins->arguments[0]->index, ins->set_zero_ptr.size);
		break;

	case IR_SET_REG:
		if (n_args == 1)
			DBG_PRINT("set_reg %s %d", get_reg_name(ins->set_reg.register_index, 8), ins->arguments[0]->index);
		else
			DBG_PRINT("set_reg %s %d state: %d", get_reg_name(ins->set_reg.register_index, 8), ins->arguments[0]->index, ins->arguments[1]->index);
		break;

	case IR_ALLOCATE_CALL_STACK:
		DBG_PRINT("allocate_call_stack %d", ins->allocate_call_stack.change);
		break;

	case IR_LOAD_PART_ADDRESS:
		DBG_PRINT("load part address of %d with offset %d", ins->arguments[0]->index, ins->load_part.offset);
		break;

	case IR_STORE_PART_ADDRESS:
		DBG_PRINT("store part of %d with offset %d", ins->arguments[0]->index, ins->load_part.offset);
		break;

	case IR_ALLOC:
		DBG_PRINT("alloc with size %d", ins->alloc.size);
		break;

	case IR_COPY_MEMORY:
		DBG_PRINT("memcpy from %d to %d", ins->arguments[0]->index, ins->arguments[1]->index);
		break;

	case IR_PROJECT:
		DBG_PRINT("proj%d %d", ins->project.index, ins->arguments[0]->index);
		break;

	case IR_FUNCTION:
		DBG_PRINT("function %s", ins->function.name);
		break;

	case IR_COUNT:
		ICE("Invalid node type\n");
	}

	if (str) {
		if (n_args == 0) {
			DBG_PRINT("%s", str);
		} else {
			int index_0 = ins->arguments[0] ? ins->arguments[0]->index : -1;
			int index_1 = ins->arguments[1] ? ins->arguments[1]->index : -1;
			int index_2 = ins->arguments[2] ? ins->arguments[2]->index : -1;
			int index_3 = ins->arguments[3] ? ins->arguments[3]->index : -1;
			DBG_PRINT(str, index_0, index_1, index_2, index_3);
		}
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

const char *dbg_token_type(ttype tt) {
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
