#include "declaration.h"
#include "common.h"
#include "expression.h"
#include "symbols.h"
#include "expression.h"
#include "function_parser.h"

#include <preprocessor/preprocessor.h>

#include <assert.h>

// Returns previous state of the bit.
int set_sbit(struct type_specifiers *ts, int bit_n) {
	int prev = (ts->specifiers & bit_n) ? 1 : 0;

	if (prev)
		return 1;

	ts->specifiers |= bit_n;
	return 0;
}

void accept_attribute(int *is_packed) {
	if (!TACCEPT(T_KATTRIBUTE))
		return;

	TEXPECT(T_LPAR);
	TEXPECT(T_LPAR);

	const char *attribute_name = T0->str;

	TEXPECT(T_IDENT);

	if (strcmp(attribute_name, "packed") == 0) {
		*is_packed = 1;
	} else {
		NOTIMP();
	}

	TEXPECT(T_RPAR);
	TEXPECT(T_RPAR);
}

struct type_ast {
	enum {
		TAST_TERMINAL,
		TAST_POINTER,
		TAST_ARRAY,
		TAST_FUNCTION
	} type;

	union {
		struct {
			char *name; // NULL if no name.
		} terminal;

		struct {
			struct type_qualifiers tq;
		} pointer;

		struct {
			enum {
				ARR_EMPTY,
				ARR_EXPRESSION,
				ARR_STAR,
			} type;

			int is_static; // only for ARR_EXPRESSION.
			struct type_qualifiers tq;

			struct expr *expr;
		} array;

		struct {
			int n;
			struct type **types;
			var_id *arguments;
			//char **names;
			int vararg;
		} function;
	};

	struct type_ast *parent;
};

struct type_ast *parse_declarator(int *was_abstract, int *has_symbols);
struct type *ast_to_type(const struct type_specifiers *ts, const struct type_qualifiers *tq, struct type_ast *ast, char **name, int allow_tq_in_array);

int parse_struct(struct type_specifiers *ts);
int parse_enum(struct type_specifiers *ts);

int parse_specifier(struct type_specifiers *ts,
					struct storage_class_specifiers *scs,
					struct type_qualifiers *tq,
					struct function_specifiers *fs,
					struct alignment_specifiers *as,
					int *got_ts) {
#define ACCEPT_INCREMENT_TS(TOKEN, VARIABLE)		\
	if (TACCEPT(TOKEN)) {						\
		(VARIABLE)++;							\
		*got_ts = 1;							\
		return 1;								\
	}

#define ACCEPT_INCREMENT(TOKEN, VARIABLE)		\
	if (TACCEPT(TOKEN)) {						\
		(VARIABLE)++;							\
		return 1;								\
	}

	// 1000110000
	if (ts) {
		unsigned int prev = ts->specifiers;
		if ((TACCEPT(T_KVOID) && set_sbit(ts, TSF_VOID)) ||
			(TACCEPT(T_KCHAR) && set_sbit(ts, TSF_CHAR)) ||
			(TACCEPT(T_KINT) && set_sbit(ts, TSF_INT)) ||
			(TACCEPT(T_KFLOAT) && set_sbit(ts, TSF_FLOAT)) ||
			(TACCEPT(T_KDOUBLE) && set_sbit(ts, TSF_DOUBLE)) ||
			(TACCEPT(T_KSIGNED) && set_sbit(ts, TSF_SIGNED)) ||
			(TACCEPT(T_KUNSIGNED) && set_sbit(ts, TSF_UNSIGNED)) ||
			(TACCEPT(T_KBOOL) && set_sbit(ts, TSF_BOOL)) ||
			(TACCEPT(T_KCOMPLEX) && set_sbit(ts, TSF_COMPLEX)) ||
			(TACCEPT(T_KSHORT) && set_sbit(ts, TSF_SHORT)) ||
			(TACCEPT(T_KLONG) && (set_sbit(ts, TSF_LONG1) &&
								  set_sbit(ts, TSF_LONG2)))) {
			ERROR("Invalid type");
		}

		if (prev != ts->specifiers) {
			*got_ts = 1;
			return 1;
		}

		if (parse_struct(ts)) {
			*got_ts = 1;
			return 1;
		}

		if (parse_enum(ts)) {
			*got_ts = 1;
			return 1;
		}

		if (TACCEPT(T_KENUM)) {
			PRINT_POS(T0->pos);
			ERROR("Not implemented");
		}

		if (TACCEPT(T_KATOMIC)) {
			ERROR("Not implemented");
		}

		if (T0->type == T_IDENT && !*got_ts) {
			*got_ts = 1;
			char *name = T0->str;

			struct symbol_typedef *sym = symbols_get_typedef(name);

			if (sym) {
				ts->data_type = sym->data_type;
				TNEXT();
				return 1;
			}
		}
			
		if (TACCEPT(T_TYPEDEF_NAME)) {
			ERROR("Not implemented");
		}
	}
	if (scs) {
		ACCEPT_INCREMENT(T_KTYPEDEF, scs->typedef_n);
		ACCEPT_INCREMENT(T_KEXTERN, scs->extern_n);
		ACCEPT_INCREMENT(T_KSTATIC, scs->static_n);
		ACCEPT_INCREMENT(T_KTHREAD_LOCAL, scs->thread_local_n);
		ACCEPT_INCREMENT(T_KAUTO, scs->auto_n);
		ACCEPT_INCREMENT(T_KREGISTER, scs->register_n);
	}
	if (tq) {
		ACCEPT_INCREMENT(T_KCONST, tq->const_n);
		ACCEPT_INCREMENT(T_KRESTRICT, tq->restrict_n);
		ACCEPT_INCREMENT(T_KVOLATILE, tq->volatile_n);
		ACCEPT_INCREMENT(T_KATOMIC, tq->atomic_n);
	}
	if (fs) {
		ACCEPT_INCREMENT(T_KINLINE, fs->inline_n);
		ACCEPT_INCREMENT(T_KNORETURN, fs->noreturn_n);
	}
	if (as) {
		switch (T0->type) {
		case T_KALIGNAS:
			ERROR("_Alignas not implemented");
		default: break;
		}
	}
	return 0;
}

int parse_specifiers(struct type_specifiers *ts,
					 struct storage_class_specifiers *scs,
					 struct type_qualifiers *tq,
					 struct function_specifiers *fs,
					 struct alignment_specifiers *as) {

	if (ts) *ts = (struct type_specifiers){ 0 };
	if (scs) *scs = (struct storage_class_specifiers){ 0 };
	if (tq) *tq = (struct type_qualifiers){ 0 };
	if (fs) *fs = (struct function_specifiers){ 0 };
	if (as) *as = (struct alignment_specifiers){ 0 };

	if (ts)
		ts->pos = T0->pos;
	int matched = 0;
	int got_ts = 0;
	while (parse_specifier(ts, scs, tq, fs, as, &got_ts)) {
		matched = 1;
	}
	return matched;
}

int parse_enumerator(struct constant *prev, int first) {
	if (T0->type != T_IDENT)
		return 0;

	char *name = T0->str;
	TNEXT();

	struct constant val;
	if (TACCEPT(T_A)) {
		struct expr *expr = parse_assignment_expression();
		if (!expr)
			ERROR("Expected expression");
			
		struct constant *ret = expression_to_constant(expr);
		if (!ret)
			ERROR("Could not evaluate constant expression, is of type %d", expr->type);

		val = *ret;
	} else {
		if (!first)
			val = constant_increment(*prev);
		else
			val = constant_zero(type_simple(ENUM_TYPE));
	}

	struct symbol_identifier *sym =
		symbols_add_identifier(name);

	sym->type = IDENT_CONSTANT;
	sym->constant = val;

	if (!TACCEPT(T_COMMA)) {
		return 0;
	}

	*prev = val;

	return 1;
}

int parse_enum(struct type_specifiers *ts) {
	if (!TACCEPT(T_KENUM))
		return 0;

	char *name = NULL;

	if (T0->type == T_IDENT) {
		name = T0->str;
		TNEXT();
	} else {
		static int anonymous_counter = 0;
		name = allocate_printf("<enum-%d>", anonymous_counter++);
	}

	if (TACCEPT(T_LBRACE)) {
		int first = 1;
		struct constant prev;
		while (parse_enumerator(&prev, first)) {
			first = 0;
		}

		TEXPECT(T_RBRACE);

		struct enum_data *data = NULL;
		struct symbol_struct *def = symbols_get_struct_in_current_scope(name);

		if (def && def->type != STRUCT_ENUM)
			ERROR("Name not declared as enum.");

		if (!def) {
			def = symbols_add_struct(name);
			def->type = STRUCT_ENUM;
			data = register_enum();
			def->enum_data = data;
		} else {
			data = def->enum_data;
			if (data->is_complete)
				ERROR("Redeclaring struct/union");
		}

		*data = (struct enum_data){
			.name = name
		};

		ts->data_type = type_simple(ST_INT);
		return 1;
	} else {
		struct symbol_struct *def = symbols_get_struct(name);

		if(!def) {
			def = symbols_add_struct(name);

			def->enum_data = register_enum();
			def->enum_data->is_complete = 0;
			def->type = STRUCT_ENUM;
		}

		if (def->type != STRUCT_ENUM) {
			ERROR("Previously not a enum");
		}

		ts->data_type = type_simple(ST_INT);
		return 1;
	}
	return 1;
}

int parse_struct(struct type_specifiers *ts) {
	int is_union = 0, is_packed = 0;
	if (TACCEPT(T_KSTRUCT))
		is_union = 0;
	else if (TACCEPT(T_KUNION))
		is_union = 1;
	else
		return 0;
	char *name = NULL;

	accept_attribute(&is_packed);

	if (T0->type == T_IDENT) {
		name = T0->str;
		TNEXT();
	} else {
		static int anonymous_counter = 0;
		name = allocate_printf("<%d>", anonymous_counter++);
	}

	if (TACCEPT(T_LBRACE)) {
		int fields_size = 0, fields_cap = 0;
		struct field *fields = NULL;

		struct specifiers s;

		while (parse_specifiers(&s.ts, NULL, &s.tq, NULL, &s.as)) {
			int found_one = 0;
			struct type_ast *ast = NULL;
			int was_abstract = 1;
			while (1) {
				struct type *type = NULL;
				char *name = NULL;
				int bitfield = -1;
				int needs_bitfield = 0;

				if ((ast = parse_declarator(&was_abstract, 0))) {
					found_one = 1;
					if (was_abstract)
						ERROR("Can't have abstract in struct declaration");

					type = ast_to_type(&s.ts, &s.tq, ast, &name, 0);
				} else {
					needs_bitfield = 1;
				}

				if (TACCEPT(T_COLON)) {
					found_one = 1; // Bit-fields can't declare anonymous structs.

					type = ast_to_type(&s.ts, &s.tq, NULL, NULL, 0);
					struct expr *bitfield_expr = parse_assignment_expression();
					struct constant *c = expression_to_constant(
						expression_cast(bitfield_expr, type_simple(ST_INT)));
					if (!c)
						ERROR("Bit-field must be a constant expression");
					if (!type_is_simple(c->data_type, ST_INT))
						ERROR("Bit-field must an integer");
					assert(c->type == CONSTANT_TYPE);
					bitfield = c->int_d;
				} else if (needs_bitfield) {
					break;
				}

				ADD_ELEMENT(fields_size, fields_cap, fields) = (struct field) {
					.type = type,
					.name = name,
					.bitfield = bitfield
				};

				TACCEPT(T_COMMA);
			}

			TEXPECT(T_SEMI_COLON);

			if (!found_one) {
				if (s.ts.data_type->type == TY_STRUCT) {
					ADD_ELEMENT(fields_size, fields_cap, fields) = (struct field) {
						.type = s.ts.data_type,
						.name = NULL,
						.bitfield = -1
					};
				} else {
					ERROR("!!!");
				}
			}
		}

		TEXPECT(T_RBRACE);

		accept_attribute(&is_packed);

		struct struct_data *data = NULL;
		struct symbol_struct *def = symbols_get_struct_in_current_scope(name);

		if (is_union) {
			if (def && def->type != STRUCT_UNION)
				ERROR("Name not declared as union.");
		} else {
			if (def && def->type != STRUCT_STRUCT)
				ERROR("Name not declared as struct.");
		}

		if (!def) {
			def = symbols_add_struct(name);
			def->type = is_union ? STRUCT_UNION : STRUCT_STRUCT;
			data = register_struct();
			def->struct_data = data;
		} else {
			data = def->struct_data;
			if (data->is_complete) {
				PRINT_POS(T0->pos);
				ERROR("Redeclaring struct/union %s", name);
			}
		}

		*data = (struct struct_data) {
			.n = fields_size,
			.is_complete = 1,
			.is_union = is_union,
			.is_packed = is_packed,

			.fields = fields,

			.name = name
		};

		calculate_offsets(data);
		type_merge_anonymous_substructures(data);

		struct type params = {
			.type = TY_STRUCT,
			.struct_data = data,
			.n = 0
		};

		ts->data_type = type_create(&params, NULL);

		return 1;
	} else {
		struct symbol_struct *def = symbols_get_struct(name);

		if(!def) {
			def = symbols_add_struct(name);

			def->struct_data = register_struct();
			*def->struct_data = (struct struct_data) {
				.name = name,
				.is_complete = 0,
			};
			def->type = is_union ? STRUCT_UNION : STRUCT_STRUCT;
		}

		if (!is_union && def->type != STRUCT_STRUCT) {
			PRINT_POS(T0->pos);
			ERROR("%s Previously not a struct", name);
		} else if (is_union && def->type != STRUCT_UNION) {
			ERROR("Previously not a union");
		}

		struct type params = {
			.type = TY_STRUCT,
			.struct_data = def->struct_data,
			.n = 0
		};

		ts->data_type = type_create(&params, NULL);
		return 1;
	}
	return 1;
}

void ast_get_parameters(struct type_ast *ast,
						int *n, var_id **arguments) {
	while (ast->type != TAST_TERMINAL) {
		switch (ast->type) {
		case TAST_POINTER:
			ast = ast->parent;
			break;

		case TAST_ARRAY:
			ast = ast->parent;
			break;

		case TAST_FUNCTION:
			if (ast->parent->type == TAST_TERMINAL) {
				*n = ast->function.n;
				*arguments = ast->function.arguments;
				return;
			} else {
				ast = ast->parent;
			}
			break;

		default:
			ERROR("Not implemented");
		}
	}
	ERROR("Did not find parameter names");
}

struct type_ast *type_ast_new(struct type_ast ast) {
	struct type_ast *ret = malloc(sizeof (struct type_ast));
	*ret = ast;
	return ret;
}

static struct type *specifiers_to_type(const struct type_specifiers *ts) {
	enum simple_type int_st = ST_INT, long_st = ST_LONG, llong_st = ST_LLONG,
		uint_st = ST_UINT, ulong_st = ST_ULONG, ullong_st = ST_ULLONG;

	switch (parser_flags.dmodel) {
	case DMODEL_LP64:
		int_st = ST_INT, long_st = ST_LONG, llong_st = ST_LLONG,
			uint_st = ST_UINT, ulong_st = ST_ULONG, ullong_st = ST_ULLONG;
		break;

	case DMODEL_ILP64:
		int_st = ST_LONG, long_st = ST_LONG, llong_st = ST_LLONG,
			uint_st = ST_ULONG, ulong_st = ST_ULONG, ullong_st = ST_ULLONG;
		break;

	case DMODEL_LLP64:
		int_st = ST_INT, long_st = ST_INT, llong_st = ST_LLONG,
			uint_st = ST_UINT, ulong_st = ST_UINT, ullong_st = ST_ULLONG;
		break;
	}

#define SPEC(A, B) if (ts->specifiers == (B)) return type_simple(A);
	SPEC(ST_VOID, TSF_VOID);
	SPEC(ST_CHAR, TSF_CHAR);
	SPEC(ST_SCHAR, TSF_SIGNED | TSF_CHAR);
	SPEC(ST_UCHAR, TSF_UNSIGNED | TSF_CHAR);

	SPEC(ST_SHORT, TSF_SHORT);
	SPEC(ST_SHORT, TSF_SIGNED | TSF_SHORT);
	SPEC(ST_SHORT, TSF_SHORT | TSF_INT);
	SPEC(ST_SHORT, TSF_SIGNED | TSF_SHORT | TSF_INT);

	SPEC(ST_USHORT, TSF_UNSIGNED | TSF_SHORT);
	SPEC(ST_USHORT, TSF_UNSIGNED | TSF_SHORT | TSF_INT);

	SPEC(int_st, TSF_INT);
	SPEC(int_st, TSF_SIGNED);
	SPEC(int_st, TSF_INT | TSF_SIGNED);

	SPEC(uint_st, TSF_UNSIGNED);
	SPEC(uint_st, TSF_UNSIGNED | TSF_INT);

	SPEC(long_st, TSF_LONG1);
	SPEC(long_st, TSF_LONG1 | TSF_SIGNED);
	SPEC(long_st, TSF_LONG1 | TSF_INT);
	SPEC(long_st, TSF_LONG1 | TSF_INT | TSF_SIGNED);

	SPEC(ulong_st, TSF_LONG1 | TSF_UNSIGNED);
	SPEC(ulong_st, TSF_LONG1 | TSF_INT | TSF_UNSIGNED);

	SPEC(llong_st, TSF_LONGB);
	SPEC(llong_st, TSF_LONGB | TSF_SIGNED);
	SPEC(llong_st, TSF_LONGB | TSF_INT);
	SPEC(llong_st, TSF_LONGB | TSF_INT | TSF_SIGNED);

	SPEC(ullong_st, TSF_LONGB | TSF_UNSIGNED);
	SPEC(ullong_st, TSF_LONGB | TSF_INT | TSF_UNSIGNED);

	SPEC(ST_FLOAT, TSF_FLOAT);
	SPEC(ST_DOUBLE, TSF_DOUBLE);
	SPEC(ST_DOUBLE, TSF_DOUBLE | TSF_LONG1); // SPEC(ST_LDOUBLE, TSF_DOUBLE | TSF_LONG1);
	SPEC(ST_BOOL, TSF_BOOL);
	SPEC(ST_FLOAT_COMPLEX, TSF_FLOAT | TSF_COMPLEX);
	SPEC(ST_DOUBLE_COMPLEX, TSF_DOUBLE | TSF_COMPLEX);
	SPEC(ST_LDOUBLE_COMPLEX, TSF_DOUBLE | TSF_LONG1 | TSF_COMPLEX);
#undef SPEC

	if (ts->data_type)
		return ts->data_type;

	PRINT_POS(ts->pos);
	ERROR("Invalid type %X", ts->specifiers);
}

int null_type_qualifier(struct type_qualifiers *tq) {
	return tq->atomic_n == 0 &&
		tq->const_n == 0 &&
		tq->restrict_n == 0 &&
		tq->volatile_n == 0;
}

struct type *apply_tq(struct type *type, const struct type_qualifiers *tq) {
	if (tq->const_n == 1)
		type = type_make_const(type, 1);
	return type;
}

struct type *ast_to_type(const struct type_specifiers *ts, const struct type_qualifiers *tq, struct type_ast *ast, char **name, int allow_tq_in_array) {
	struct type *type = specifiers_to_type(ts);

	type = apply_tq(type, tq);

	if (!ast)
		return type;

	while (ast->type != TAST_TERMINAL) {
		switch (ast->type) {
		case TAST_POINTER:
			type = type_pointer(type);
			type = apply_tq(type, &ast->pointer.tq);
			ast = ast->parent;
			break;

		case TAST_FUNCTION: {
			struct type params = {
				.type = TY_FUNCTION,
				.function.is_variadic = ast->function.vararg,
				.n = ast->function.n + 1
			};

			struct type *parameters[ast->function.n + 1];
			for (int i = 0; i < ast->function.n; i++)
				parameters[i + 1] = ast->function.types[i];
			parameters[0] = type;

			type = type_create(&params, parameters);

			ast = ast->parent;
		} break;

		case TAST_ARRAY: {
			switch (ast->array.type) {
			case ARR_EMPTY: {
				struct type params = {
					.type = TY_INCOMPLETE_ARRAY,
					.n = 1
				};
				type = type_create(&params, &type);
			} break;
			case ARR_EXPRESSION: {
				struct constant *length;
				struct expr *length_expr = expression_cast(ast->array.expr, type_simple(SIZE_TYPE));
				if ((length = expression_to_constant(length_expr))) {
					assert(length->type == CONSTANT_TYPE);
					struct type params = {
						.type = TY_ARRAY,
						.array.length = length->ullong_d,
						.n = 1
					};
					type = type_create(&params, &type);
				} else {
					struct type params = {
						.type = TY_VARIABLE_LENGTH_ARRAY,
						.variable_length_array.length_expr = length_expr,
						.n = 1
					};
					type = type_create(&params, &type);
				}
			} break;
			case ARR_STAR: {
				struct type params = {
					.type = TY_VARIABLE_LENGTH_ARRAY,
					.variable_length_array.length_expr = NULL,
					.n = 1
				};
				type = type_create(&params, &type);
			} break;
			default:
				NOTIMP();
			}
			if (!allow_tq_in_array && !null_type_qualifier(&ast->array.tq)) {
				ERROR("Can't have type qualifiers in array outside of function prototype.");
			} else if (allow_tq_in_array) {
				type = apply_tq(type, &ast->array.tq);
			}
			ast = ast->parent;
		} break;

		default:
			ERROR("Not implemented");
		}
	}

	if (name)
		*name = ast->terminal.name;

	return type;
}

struct parameter_list {
	int abstract;
	int n;
	int vararg;
	struct type **types;
	var_id *arguments;
};

struct parameter_list parse_parameter_list(void) {
	struct parameter_list ret = { 0 };
	symbols_push_scope();

	struct specifiers s;
	int first = 1;
	while (parse_specifiers(&s.ts, &s.scs, &s.tq, &s.fs, &s.as)) {
		int was_abstract = 10;
		struct type_ast *ast = parse_declarator(&was_abstract, 0);

		if (first) {
			ret.abstract = was_abstract;
			first = 0;
		} else if (was_abstract != ret.abstract) {
			if (was_abstract == 10)
				ERROR("Something went wrong");
		}

		ret.n++;
		ret.types = realloc(ret.types, ret.n * sizeof(*ret.types));
		char *name = NULL;
		struct type *type = ast_to_type(&s.ts, &s.tq, ast, &name, 1);
		type = type_adjust_parameter(type);
		ret.types[ret.n - 1] = type;

		if (!was_abstract) {
			ret.arguments = realloc(ret.arguments, ret.n * sizeof(*ret.arguments));
			struct symbol_identifier *ident = symbols_add_identifier(name);
			ident->type = IDENT_VARIABLE;
			ident->variable.type = type;
			ident->variable.id = new_variable(type, 0, 0);
			ret.arguments[ret.n - 1] = ident->variable.id;
		}

		if (T0->type == T_RPAR)
			break;
		TEXPECT(T_COMMA);
	}

	if (TACCEPT(T_ELLIPSIS)) {
		ret.vararg = 1;
	} else {
		ret.vararg = 0;
	}

	TEXPECT(T_RPAR);

	return ret;
}

struct type_ast *parse_function_parameters(struct type_ast *parent, int *has_symbols) {
	if (TACCEPT(T_RPAR)) {
		return type_ast_new((struct type_ast){
				.type = TAST_FUNCTION,
				.function.n = 0,
				.function.vararg = 1,
				.function.types = NULL,
				.function.arguments = NULL,
				.parent = parent
			});
	}

	struct parameter_list parameters = { 0 };
	if (T0->type == T_KVOID &&
		T1->type == T_RPAR) {
		TNEXT();
		TNEXT();
	} else {
		parameters = parse_parameter_list();

		if (has_symbols && !*has_symbols)
			*has_symbols = 1;
		else
			symbols_pop_scope();
	}

	return type_ast_new((struct type_ast){
			.type = TAST_FUNCTION,
			.function.n = parameters.n,
			.function.vararg = parameters.vararg,
			.function.types = parameters.types,
			.function.arguments = parameters.arguments,
			.parent = parent
		});
}

struct type_ast *parse_declarator(int *was_abstract, int *has_symbols) {
	struct type_ast *ast = NULL;
	if (T0->type == T_IDENT) {
		ast = type_ast_new((struct type_ast){
				.type = TAST_TERMINAL,
				.terminal.name = strdup(T0->str)
			});
		if (was_abstract)
			*was_abstract = 0;
		TNEXT();
	} else if (TACCEPT(T_LPAR)) {
		if (!(T0->type == T_IDENT && symbols_get_typedef(T0->str)))
			ast = parse_declarator(was_abstract, has_symbols);
		if (!ast) {
			*was_abstract = 1;
			ast = type_ast_new((struct type_ast) {
					.type = TAST_TERMINAL,
					.terminal.name = NULL
				});
			ast = parse_function_parameters(ast, has_symbols);
		} else {
			TEXPECT(T_RPAR);
		}
	} else if (TACCEPT(T_STAR)) {
		// Read type qualifiers.

		struct type_qualifiers tq;
		parse_specifiers(NULL, NULL, &tq, NULL, NULL);

		ast = type_ast_new((struct type_ast){
				.type = TAST_POINTER,
				.parent = parse_declarator(was_abstract, has_symbols),
				.pointer.tq = tq
			});


		if (ast->parent == NULL) {
			ast->parent = type_ast_new((struct type_ast){
					.type = TAST_TERMINAL,
					.terminal.name = NULL
				});
			*was_abstract = 1;
		}
	} else if (T0->type == T_LBRACK) {
		// Small hack to allow for type-names like int[].
		*was_abstract = 1;
		ast = type_ast_new((struct type_ast) {
				.type = TAST_TERMINAL,
				.terminal.name = NULL
			});
	} else {
		*was_abstract = 1; // Is this correct?
		return NULL;
	}

	while (T0->type == T_LPAR ||
		   T0->type == T_LBRACK) {
		if (TACCEPT(T_LBRACK)) {
			struct type_ast arr;
			arr.type = TAST_ARRAY;

			int need_expression = 0;
			if (TACCEPT(T_KSTATIC)) {
				need_expression = 1;
				arr.array.is_static = 1;
			}

			parse_specifiers(NULL, NULL,
							 &arr.array.tq, NULL, NULL);

			if (TACCEPT(T_KSTATIC)) {
				need_expression = 1;
				arr.array.is_static = 1;
			}

			if (TACCEPT(T_RBRACK)) {
				arr.array.type = ARR_EMPTY;
				if (need_expression)
					ERROR("Missing expression after static");
			} else if (TACCEPT(T_STAR)) {
				arr.array.type = ARR_STAR;
				TEXPECT(T_RBRACK);
			} else {
				struct expr *expression = parse_expression();
				if (!expression)
					ERROR("Didn't find expression");

				arr.array.type = ARR_EXPRESSION;
				arr.array.expr = expression;

				TEXPECT(T_RBRACK);
			}

			arr.parent = ast;
			ast = type_ast_new(arr);
		} else if (TACCEPT(T_LPAR)) {
			ast = parse_function_parameters(ast, has_symbols);
		}
	}

	return ast;
}

static struct initializer *initializer_init(void) {
	struct initializer *init = malloc(sizeof *init);
	*init = (struct initializer) { 0 };

	return init;
}

void add_expr_init_pair(struct initializer *init, int offset, int bit_offset, int bit_size, struct expr *expr) {
	ADD_ELEMENT(init->size, init->cap, init->pairs) = (struct init_pair) {
		.type = IP_EXPRESSION,
		.offset = offset,
		.bit_offset = bit_offset,
		.bit_size = bit_size,
		.u.expr = expr
	};
}

void add_string_init_pair(struct initializer *init, int offset, const char *str) {
	ADD_ELEMENT(init->size, init->cap, init->pairs) = (struct init_pair) {
		.type = IP_STRING,
		.offset = offset,
		.u.str = str
	};
}

int parse_non_brace_initializer(struct type **type, int offset, int bit_offset,
								int bitfield, struct initializer *init,
	struct expr **failed_expr) {
	if (failed_expr)
		*failed_expr = NULL;
	if (is_scalar(*type)) {
		int has_braces = TACCEPT(T_LBRACE);

		struct expr *expr = parse_assignment_expression();
		if (!expr) {
			printf("%s\n%d\n", dbg_type(*type), has_braces);
			PRINT_POS(T0->pos);
			ERROR("Expected expression, got: %s", dbg_token(T0));
		}

		if (expr->data_type->type == TY_STRUCT) {
			if (!failed_expr) {
				ERROR("Got invalid rhs for type %s\n", dbg_type(*type));
			}
			assert(failed_expr);
			*failed_expr = expr;
			return 0;
		} else {
			struct expr *casted_expr = expression_cast(expr, *type);
			if (!expression_is_zero(casted_expr)) {
				add_expr_init_pair(init, offset, bit_offset, bitfield, expression_cast(expr, *type));
			}
		}

		if (has_braces)
			TEXPECT(T_RBRACE);
		return 1;
	} else if (((*type)->type == TY_ARRAY || (*type)->type == TY_INCOMPLETE_ARRAY) &&
			   (type_is_simple((*type)->children[0], ST_CHAR) ||
				type_is_simple((*type)->children[0], ST_UCHAR) ||
				type_is_simple((*type)->children[0], ST_SCHAR))) {
		char *str;
		if (T0->type == T_LBRACE && T1->type == T_STRING &&
			T2->type == T_RBRACE) {
			str = T1->str;
			TNEXT();
			TNEXT();
			TNEXT();
		} else if (T0->type == T_STRING) {
			str = T0->str;
			TNEXT();
		} else {
			return 0;
		}

		if ((*type)->type == TY_INCOMPLETE_ARRAY) {
			struct type complete_array_params = {
				.type = TY_ARRAY,
				.array.length = strlen(str) + 1,
				.n = 1
			};

			*type = type_create(&complete_array_params, (*type)->children);
		}

		add_string_init_pair(init, offset, str);
		return 1;
	}

	return 0;
}

int type_is_char_array(struct type *type) {
	return (type->type == TY_ARRAY || type->type == TY_INCOMPLETE_ARRAY) &&
		type_is_simple(type->children[0], ST_CHAR);
}

// 6.7.9p17-6.7.8p23 and some surrounding paragraph.
// TODO: Too many for(;;) in this function.
int parse_brace_initializer(struct type **current_object, int offset, struct initializer *init) {
	if (!TACCEPT(T_LBRACE))
		return 0;

	int stack_count = 1;
	int index_stack[32]; // TODO: Remove arbitrary (but very reasonable) limit.
	int offset_stack[32];
	struct type *type_stack[32];

	index_stack[0] = 0;
	type_stack[0] = *current_object;
	offset_stack[0] = offset;

	int max_index = 0;

	int must_have_designator_list = 0;

	do {
		int has_designator_list = 0;
		struct type *parent_type = *current_object;
		int parent_offset = offset;
		for (;;) {
			if (T0->type == T_DOT || T0->type == T_LBRACK) {
				has_designator_list = 1;
				stack_count = 0;
			}

			int index;

			if (TACCEPT(T_DOT)) {
				char *ident = T0->str;
				TEXPECT(T_IDENT);

				index = type_member_idx(parent_type, ident);
			} else if (TACCEPT(T_LBRACK)) {
				struct expr *expr = parse_expression();
				TEXPECT(T_RBRACK);

				struct constant *constant = expression_to_constant(expression_cast(expr, type_simple(ST_INT)));
				if (!constant)
					ERROR("Array designator must have constant expression.");
				assert(type_is_simple(constant->data_type, ST_INT));

				index = constant->int_d;
			} else {
				break;
			}

			int noffset;
			struct type *ntype;
			type_select(parent_type, index, &noffset, &ntype);

			stack_count++;
			index_stack[stack_count - 1] = index;
			type_stack[stack_count - 1] = parent_type;
			offset_stack[stack_count - 1] = parent_offset;

			parent_type = ntype;
			parent_offset += noffset;
		}

		if (must_have_designator_list && !has_designator_list) {
			break;
		}

		must_have_designator_list = 0;

		if (has_designator_list)
			TEXPECT(T_A);

		if (type_is_simple(type_stack[stack_count - 1], ST_CHAR)) {
			PRINT_POS(T0->pos);
			ERROR("Should not be here");
		}
		struct type *selected_type = NULL;
		int selected_offset = 0, selected_bit_offset = 0, selected_bitfield = -1;
		type_select(type_stack[stack_count - 1], index_stack[stack_count - 1], &selected_offset, &selected_type);
		if (type_stack[stack_count - 1]->type == TY_STRUCT) {
			int index = index_stack[stack_count - 1];
			struct struct_data *data = type_stack[stack_count - 1]->struct_data;
			selected_bit_offset = data->fields[index].bit_offset;
			selected_bitfield = data->fields[index].bitfield;
		}
		selected_offset += offset_stack[stack_count - 1];

		if (type_is_char_array(selected_type) && parse_non_brace_initializer(&selected_type,
																			 selected_offset,
																			 selected_bit_offset,
																			 selected_bitfield,
																			 init, NULL)) {
		} else if (!(type_is_aggregate(selected_type) &&
			  parse_brace_initializer(&selected_type, selected_offset, init))) {
			// Recurse down until parse_non_brace_initializer matches.
			int start_count = stack_count;
			int found = 0;

			struct type *current = selected_type;
			while (type_is_aggregate(current)) {
				struct expr *failed_expr = NULL;
				if (parse_non_brace_initializer(&current, selected_offset, selected_bit_offset, selected_bitfield,
												init, &failed_expr)) {
					found = 1;
					break;
				}

				stack_count++;

				type_stack[stack_count - 1] = current;
				index_stack[stack_count - 1] = 0;
				offset_stack[stack_count - 1] = selected_offset;

				type_select(current, 0, NULL, &current);
			}

			struct expr *failed_expr = NULL;
			if (!found && parse_non_brace_initializer(&current, selected_offset, selected_bit_offset,
													  selected_bitfield,
													  init, &failed_expr)) {
				found = 1;
			}

			if (!found) {
				// This 
				struct expr *expr = failed_expr ? failed_expr : parse_assignment_expression();
				if (!expr)
					ERROR("Expected expression");
				if (!type_is_aggregate(expr->data_type))
					ERROR("Expected expression of aggregate data type, but got %s", dbg_type(expr->data_type));

				for (; stack_count > start_count; stack_count--) {
					if (expr->data_type == type_stack[stack_count - 1]) {
						add_expr_init_pair(init, selected_offset, selected_bit_offset, selected_bitfield, expr);
						break;
					}
				}

				if (stack_count == start_count) {
					ERROR("Can't cast aggregate type");
				}

				stack_count--;
			}
		}

		max_index = MAX(max_index, index_stack[0]);

		// Advance one step.
		while (stack_count > 0) {
			int idx = ++index_stack[stack_count - 1];
			struct type *top_type = type_stack[stack_count - 1];
			int num_members = 0;
			switch (top_type->type) {
			case TY_ARRAY:
				num_members = top_type->array.length;
				break;
			case TY_INCOMPLETE_ARRAY:
				num_members = -1;
				break;
			case TY_STRUCT:
				num_members = top_type->struct_data->n;
				// Advance until end of struct, or reaching a different offset.
				while (idx < num_members && (top_type->struct_data->fields[idx - 1].offset ==
											 top_type->struct_data->fields[idx].offset &&
											 top_type->struct_data->fields[idx - 1].bit_offset ==
											 top_type->struct_data->fields[idx].bit_offset)) {
					idx = ++index_stack[stack_count - 1];
				}
				break;
			default: ERROR("Invalid top type");
			}

			if (num_members >= 0 &&
				index_stack[stack_count - 1] >= num_members) {
				stack_count--;
			} else {
				break;
			}
		}

		if (stack_count == 0) {
			must_have_designator_list = 1;
			TACCEPT(T_COMMA);
			continue;
		}

		if (!TACCEPT(T_COMMA)) {
			break;
		}
		if (T0->type == T_RBRACE) {
			break;
		}
	} while (1);

	if ((*current_object)->type == TY_INCOMPLETE_ARRAY) {
		struct type complete_array_params = {
			.type = TY_ARRAY,
			.array.length = max_index + 1,
			.n = 1
		};

		struct type *ntype = type_create(&complete_array_params, (*current_object)->children);
		*current_object = ntype;
	}

	if (stack_count == 0)
		TACCEPT(T_COMMA);

	TEXPECT(T_RBRACE);
	return 1;
}

struct initializer *parse_initializer(struct type **type) {
	struct initializer *init = initializer_init();

	if (!parse_non_brace_initializer(type, 0, 0, -1, init, NULL)) {
		struct expr *expr = parse_assignment_expression();

		if (expr) {
			add_expr_init_pair(init, 0, 0, -1, expression_cast(expr, *type));
		} else {
			parse_brace_initializer(type, 0, init);
		}
	}

	if ((*type)->type == TY_INCOMPLETE_ARRAY) {
		PRINT_POS(T0->pos);
		ERROR("Should have completed");
	}

	return init;
}

struct type *parse_type_name(void) {
	struct specifiers s = {0};
	int has_spec = parse_specifiers(&s.ts, NULL, &s.tq, NULL, &s.as);

	if (!has_spec)
		return NULL;

	int was_abstract = 1;
	struct type_ast *ast = parse_declarator(&was_abstract, 0);
	if (!was_abstract)
		ERROR("Type name must be abstract");

	struct type *type = ast_to_type(&s.ts, &s.tq, ast, NULL, 0);
	type_evaluate_vla(type);
	return type;
}

struct {
	size_t size, cap;
	const char **names;
} potentially_tentative;

int parse_init_declarator(struct specifiers s, int external, int *was_func) {
	*was_func = 0;
	int was_abstract = 1, has_symbols = 0;
	struct type_ast *ast = parse_declarator(&was_abstract, &has_symbols);

	if (!ast)
		return 0;

	if (was_abstract) {
		PRINT_POS(T0->pos);
		ERROR("\nDeclaration can't be abstract");
	}

	struct type *type;
	char *name;
	type = ast_to_type(&s.ts, &s.tq, ast, &name, 0);

	if (T0->type == T_LBRACE) {
		int arg_n = 0;
		var_id *args = NULL;
		ast_get_parameters(ast, &arg_n, &args);

		if (!has_symbols)
			symbols_push_scope();

		if (!external)
			ERROR("Function definition are not allowed inside functions.");

		if (arg_n && !args)
			ERROR("Should not be null");

		parse_function(name, type, arg_n, args, s.scs.static_n ? 0 : 1);
		*was_func = 1;
		return 1;
	}

	// Pop function prototype scope if it is not needed for function definition.
	if (has_symbols)
		symbols_pop_scope();

	// Evaluate all unevaluated VLAs.
	if (external) {
		if (type_contains_unevaluated_vla(type))
			ERROR("Global declaration can't contain VLA.");
	} else {
		type_evaluate_vla(type);
	}

	if (s.scs.typedef_n) {
		symbols_add_typedef(name)->data_type = type;
		return 1;
	}

	// Check for agreement with previous declarations.
	struct symbol_identifier *symbol = symbols_get_identifier_in_current_scope(name);
	int prev_definition = 0;

	if (symbol) {
		prev_definition = symbol->has_definition;
		struct type *prev_type = symbols_get_identifier_type(symbol);
		struct type *composite_type = type_make_composite(type, prev_type);

		if (!composite_type)
			ERROR("Conflicting types: %s and %s\n", strdup(dbg_type(prev_type)),
				  strdup(dbg_type(type)));

		type = composite_type;
	} else {
		symbol = symbols_add_identifier(name);
	}
	
	int has_init = TACCEPT(T_A);

	// Handle function definitions first, since they differ a bit from the others.
	if (type->type == TY_FUNCTION) {
		if (has_init)
			ERROR("Function definition can't have initializer.");
		if (!external && s.scs.static_n)
			ERROR("Function declarations outside file-scope can't be static.");

		symbol->type = IDENT_LABEL;
		symbol->label.name = name;
		symbol->label.type = type;

		return 1;
	}

	int definition_is_static = 0;
	int has_definition = 0;
	int is_global = 0;
	int is_tentative = 0;

	if (s.scs.extern_n && has_init)
		ERROR("Extern declaration can't have initializer.");

	if (s.scs.register_n)
		symbol->is_register = 1;

	if (external) {
		definition_is_static = 1;

		if (!s.scs.static_n)
			is_global = 1;

		if (s.scs.extern_n) {
			has_definition = 0;
		} else if (has_init) {
			has_definition = 1;
		} else {
			is_tentative = 1;
		}
	} else {
		if (s.scs.static_n)
			definition_is_static = 1;
		if (s.scs.extern_n) {
			has_definition = 0;
			is_global = 0;
		} else {
			has_definition = 1;
		}
	}

	if (!prev_definition) {
		symbol->is_tentative = is_tentative;
		symbol->is_global = is_global;
		symbol->has_definition = has_definition;
	} else {
		if (!is_tentative && has_definition)
			symbol->is_tentative = 0;
	}
	if (is_tentative && !prev_definition)
		ADD_ELEMENT(potentially_tentative.size, potentially_tentative.cap, potentially_tentative.names) = strdup(name);

	if (has_definition) {
		if (definition_is_static) {
			symbol->type = IDENT_LABEL;

			if (!external) {
				static int local_var = 0;
				name = allocate_printf(".LVAR%d%s", local_var++, name);
			}

			symbol->label.name = name;
			symbol->label.type = type;

			struct initializer *init = NULL;
			if (has_init) {
				init = parse_initializer(&type);
				symbol->label.type = type;
			}
			data_register_static_var(name, type, init, is_global);
		} else {
			if (type_has_variable_size(type)) {
				symbol->type = IDENT_VARIABLE_LENGTH_ARRAY;
				symbol->variable_length_array.id = allocate_vla(type);
				symbol->variable_length_array.type = type;

				if (has_init)
					ERROR("Variable length array can't have initializer");
			} else {
				symbol->type = IDENT_VARIABLE;
				symbol->variable.id = new_variable(type, 1, 0);
				symbol->variable.type = type;
				if (has_init) {
					struct initializer *init = parse_initializer(&type);
					symbol->variable.type = type;
					change_variable_size(symbol->variable.id, calculate_size(type));
					ir_init_var(init, symbol->variable.id);
				}
			}
		}
	} else {
		symbol->type = IDENT_LABEL;
		symbol->label.name = name;
		symbol->label.type = type;
	}

	return 1;
}

int parse_declaration(int external) {
	struct specifiers s;
	if (!parse_specifiers(&s.ts,
						  &s.scs,
						  &s.tq,
						  &s.fs,
						  &s.as)) {
		if (TACCEPT(T_KSTATIC_ASSERT)) {
			TEXPECT(T_LPAR);
			struct expr *expr = EXPR_BINARY_OP(OP_EQUAL, EXPR_INT(0), parse_assignment_expression());
			struct constant *constant = expression_to_constant(expr);
			if (!constant) {
				ERROR("Expresison in _Static_assert must be constant.");
			}

			assert(constant->type == CONSTANT_TYPE);

			if (!type_is_simple(constant->data_type, ST_INT)) {
				ERROR("Invalid _Static_assert type: %s\n", dbg_type(constant->data_type));
			}

			TEXPECT(T_COMMA);
			if (T0->type != T_STRING)
				ERROR("Second argument to _Static_assert must be a string.");

			const char *msg = T0->str;
			TNEXT();

			TEXPECT(T_RPAR);

			TEXPECT(T_SEMI_COLON);

			if (constant->int_d)
				ERROR("Static assert failed: %s\n", msg);
			return 1;
		}
		return 0;
	}

	int was_func = 0;
	while (!was_func && parse_init_declarator(s, external, &was_func)) {
		TACCEPT(T_COMMA);
	}

	if (!was_func)
		TEXPECT(T_SEMI_COLON);

	return 1;
}

void generate_tentative_definitions(void) {
	for (size_t i = 0; i < potentially_tentative.size; i++) {
		const char *name = potentially_tentative.names[i];

		struct symbol_identifier *symbol = symbols_get_identifier_global(name);

		if (symbol && symbol->is_tentative) {
			struct type *type = symbols_get_identifier_type(symbol);
			data_register_static_var(name, type, NULL, symbol->is_global);
			symbol->is_tentative = 0;
		}
	}
}
