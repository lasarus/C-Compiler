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
			char **names;
			int vararg;
		} function;
	};

	struct type_ast *parent;
};

struct type_ast *parse_declarator(int *was_abstract);
struct type *ast_to_type(const struct type_specifiers *ts, struct type_ast *ast, char **name);

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
			PRINT_POS(TPEEK(0)->pos);
			ERROR("Not implemented");
		}

		if (TACCEPT(T_KATOMIC)) {
			ERROR("Not implemented");
		}

		if (TPEEK(0)->type == T_IDENT && !*got_ts) {
			*got_ts = 1;
			char *name = TPEEK(0)->str;

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
		switch (TPEEK(0)->type) {
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
		ts->pos = TPEEK(0)->pos;
	int matched = 0;
	int got_ts = 0;
	while (parse_specifier(ts, scs, tq, fs, as, &got_ts)) {
		matched = 1;
	}
	return matched;
}

int parse_enumerator(struct constant *prev, int first) {
	if (TPEEK(0)->type != T_IDENT)
		return 0;

	char *name = TPEEK(0)->str;
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

	if (TPEEK(0)->type == T_IDENT) {
		name = TPEEK(0)->str;
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
	int is_union = 0;
	if (TACCEPT(T_KSTRUCT))
		is_union = 0;
	else if (TACCEPT(T_KUNION))
		is_union = 1;
	else
		return 0;
	char *name = NULL;

	if (TPEEK(0)->type == T_IDENT) {
		name = TPEEK(0)->str;
		TNEXT();
	} else {
		static int anonymous_counter = 0;
		name = allocate_printf("<%d>", anonymous_counter++);
	}

	if (TACCEPT(T_LBRACE)) {
		char **names = NULL;
		struct type **types = NULL;
		int n = 0;

		struct specifiers s;

		while (parse_specifiers(&s.ts, NULL, &s.tq, NULL, &s.as)) {
			int found_one = 0;
			struct type_ast *ast = NULL;
			int was_abstract = 1;
			while ((ast = parse_declarator(&was_abstract))) {
				found_one = 1;
				if (was_abstract)
					ERROR("Can't have abstract in struct declaration");

				n++;
				types = realloc(types, n * sizeof(*types));
				names = realloc(names, n * sizeof(*names));

				types[n - 1] = ast_to_type(&s.ts, ast, &names[n - 1]);
				TACCEPT(T_COMMA);
			}

			TEXPECT(T_SEMI_COLON);

			if (!found_one) {
				if (s.ts.data_type->type == TY_STRUCT) {
					n++;
					types = realloc(types, n * sizeof(*types));
					names = realloc(names, n * sizeof(*names));
					names[n - 1] = NULL;
					types[n - 1] = s.ts.data_type;
				} else {
					ERROR("!!!");
				}
			}
		}

		TEXPECT(T_RBRACE);

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
				PRINT_POS(TPEEK(0)->pos);
				ERROR("Redeclaring struct/union %s", name);
			}
		}

		*data = (struct struct_data) {
			.n = n,
			.is_complete = 1,
			.is_union = is_union,

			.names = names,
			.types = types,

			.name = name
		};

		calculate_offsets(data);
		merge_anonymous(data);

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
			PRINT_POS(TPEEK(0)->pos);
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
						int *n, char ***names) {
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
				*names = ast->function.names;
				return;
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

struct type *specifiers_to_type(const struct type_specifiers *ts) {
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

	SPEC(ST_INT, TSF_INT);
	SPEC(ST_INT, TSF_SIGNED);
	SPEC(ST_INT, TSF_INT | TSF_SIGNED);

	SPEC(ST_UINT, TSF_UNSIGNED);
	SPEC(ST_UINT, TSF_UNSIGNED | TSF_INT);

	SPEC(ST_LONG, TSF_LONG1);
	SPEC(ST_LONG, TSF_LONG1 | TSF_SIGNED);
	SPEC(ST_LONG, TSF_LONG1 | TSF_INT);
	SPEC(ST_LONG, TSF_LONG1 | TSF_INT | TSF_SIGNED);

	SPEC(ST_ULONG, TSF_LONG1 | TSF_UNSIGNED);
	SPEC(ST_ULONG, TSF_LONG1 | TSF_INT | TSF_UNSIGNED);

	SPEC(ST_LLONG, TSF_LONGB);
	SPEC(ST_LLONG, TSF_LONGB | TSF_SIGNED);
	SPEC(ST_LLONG, TSF_LONGB | TSF_INT);
	SPEC(ST_LLONG, TSF_LONGB | TSF_INT | TSF_SIGNED);

	SPEC(ST_ULLONG, TSF_LONGB | TSF_UNSIGNED);
	SPEC(ST_ULLONG, TSF_LONGB | TSF_INT | TSF_UNSIGNED);

	SPEC(ST_FLOAT, TSF_FLOAT);
	SPEC(ST_DOUBLE, TSF_DOUBLE);
	SPEC(ST_LDOUBLE, TSF_DOUBLE | TSF_LONG1);
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

struct type *ast_to_type(const struct type_specifiers *ts, struct type_ast *ast, char **name) {
	struct type *type = specifiers_to_type(ts);

	if (!ast)
		return type;

	while (ast->type != TAST_TERMINAL) {
		switch (ast->type) {
		case TAST_POINTER:
			type = type_pointer(type);
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
				if ((length = expression_to_constant(ast->array.expr))) {
					assert(length->type == CONSTANT_TYPE);
					assert(type_is_simple(length->data_type, ST_INT));
					struct type params = {
						.type = TY_ARRAY,
						.array.length = length->int_d,
						.n = 1
					};
					type = type_create(&params, &type);
				} else {
					// VLA. This is a bit tricky.
					// Normally, IR isn't generated during type parsing.
					// This is an exception.
					// TODO: Safeguard against VLA in global scope.
					// TODO: Don't cast to integer.
					var_id length = expression_to_ir(expression_cast(ast->array.expr, type_simple(ST_INT)));

					struct type params = {
						.type = TY_VARIABLE_LENGTH_ARRAY,
						.variable_length_array.length = length,
						.n = 1
					};
					type = type_create(&params, &type);
				}
			} break;
			default:
				NOTIMP();
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
	char **names;
};

struct parameter_list parse_parameter_list(void) {
	struct parameter_list ret =
		{
			.n = 0,
			.vararg = 0,
			.types = NULL,
			.names = NULL
		};

	struct specifiers s;
	int first = 1;
	while (parse_specifiers(&s.ts, &s.scs, &s.tq, &s.fs, &s.as)) {
		int was_abstract = 10;
		struct type_ast *ast = parse_declarator(&was_abstract);

		if (first) {
			ret.abstract = was_abstract;
			first = 0;
		} else if (was_abstract != ret.abstract) {
			if (was_abstract == 10) {
				ERROR("SOmething went wrong");
			}
			ERROR("Abstractness can't be mixed in parameter list");
		}


		ret.n++;
		ret.types = realloc(ret.types, ret.n * sizeof(*ret.types));
		char *name = NULL;;
		ret.types[ret.n - 1] = ast_to_type(&s.ts, ast, &name);
		ret.types[ret.n - 1] = parameter_adjust(ret.types[ret.n - 1]);
		if (!ret.abstract) {
			ret.names = realloc(ret.names, ret.n * sizeof(*ret.names));
			ret.names[ret.n - 1] = name;
		}

		TACCEPT(T_COMMA);
	}

	if (TACCEPT(T_ELLIPSIS)) {
		ret.vararg = 1;
	} else {
		ret.vararg = 0;
	}

	TEXPECT(T_RPAR);

	return ret;
}

struct type_ast *parse_function_parameters(struct type_ast *parent) {
	if (TACCEPT(T_RPAR)) {
		return type_ast_new((struct type_ast){
				.type = TAST_FUNCTION,
				.function.n = 0,
				.function.vararg = 1,
				.function.types = NULL,
				.function.names = NULL,
				.parent = parent
			});
	}

	struct parameter_list parameters = { 0 };
	if (TPEEK(0)->type == T_KVOID &&
		TPEEK(1)->type == T_RPAR) {
		TNEXT();
		TNEXT();
	} else {
		parameters = parse_parameter_list();
	}

	return type_ast_new((struct type_ast){
			.type = TAST_FUNCTION,
			.function.n = parameters.n,
			.function.vararg = parameters.vararg,
			.function.types = parameters.types,
			.function.names = parameters.names,
			.parent = parent
		});
}

struct type_ast *parse_declarator(int *was_abstract) {
	struct type_ast *ast = NULL;
	if (TPEEK(0)->type == T_IDENT) {
		ast = type_ast_new((struct type_ast){
				.type = TAST_TERMINAL,
				.terminal.name = strdup(TPEEK(0)->str)
			});
		if (was_abstract)
			*was_abstract = 0;
		TNEXT();
	} else if (TACCEPT(T_LPAR)) {
		ast = parse_declarator(was_abstract);
		if (!ast) {
			// This is really a function declaration.
			ERROR("Not implemented");
		}
		TEXPECT(T_RPAR);
	} else if (TACCEPT(T_STAR)) {
		// Read type qualifiers.

		struct type_qualifiers tq;
		parse_specifiers(NULL, NULL, &tq, NULL, NULL);

		ast = type_ast_new((struct type_ast){
				.type = TAST_POINTER,
				.parent = parse_declarator(was_abstract),
				.pointer.tq = tq
			});


		if (ast->parent == NULL) {
			ast->parent = type_ast_new((struct type_ast){
					.type = TAST_TERMINAL,
					.terminal.name = NULL
				});
			*was_abstract = 1;
		}
	} else {
		*was_abstract = 1; // Is this correct?
		return NULL;
	}

	while (TPEEK(0)->type == T_LPAR ||
		   TPEEK(0)->type == T_LBRACK) {
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
			ast = parse_function_parameters(ast);
		}
	}

	return ast;
}

static struct initializer *initializer_init(void) {
	struct initializer *init = malloc(sizeof *init);
	*init = (struct initializer) { 0 };

	return init;
}

void add_init_pair(struct initializer *init, struct init_pair pair) {
	init->n++;
	init->pairs = realloc(init->pairs, sizeof (*init->pairs) * init->n);
	init->pairs[init->n - 1] = pair;
}

int parse_designator_list(int *first_index, int *offset, struct type **type) {
	int first = 0;
	for (;;first = 1) {
		if (TACCEPT(T_DOT)) {
			char *ident = TPEEK(0)->str;
			TEXPECT(T_IDENT);

			assert((*type)->type == TY_STRUCT);

			int mem_idx = type_member_idx(*type, ident);

			int noffset;
			struct type *ntype;
			type_select(*type, mem_idx, &noffset, &ntype);

			*offset += noffset;
			*type = ntype;

			if (!first)
				*first_index = mem_idx;
		} else if (TACCEPT(T_LBRACK)) {
			struct expr *expr = parse_expression();
			TEXPECT(T_RBRACK);

			struct constant *constant = expression_to_constant(expr);
			assert(constant);

			size_t mem_idx = 0;
			switch (constant->type) {
			case CONSTANT_TYPE:
				switch (constant->data_type->type) {
				case TY_SIMPLE:
					switch (constant->data_type->simple) {
					case ST_INT:
						mem_idx = constant->int_d;
						break;

					default:
						NOTIMP();
					}
					break;
				
				default:
					NOTIMP();
					break;
				}
				break;
			default:
				NOTIMP();
			}

			int noffset;
			struct type *ntype;
			type_select(*type, mem_idx, &noffset, &ntype);

			*offset += noffset;
			*type = ntype;

			if (!first)
				*first_index = mem_idx;
		} else {
			break;
		}
	}

	return first;
}

void parse_initializer_recursive(int offset, struct type **type, int set_type,
								 struct initializer *initializer) {
	if (TACCEPT(T_LBRACE)) {
		if (T0->type == T_NUM &&
			strcmp(T0->str, "0") == 0 &&
			TPEEK(1)->type == T_RBRACE) {
			TNEXT();
			TNEXT();
			return;
		}

		int current_member = 0, max_member = 0;
		while (!TACCEPT(T_RBRACE)) {
			int noffset = 0;
			struct type *ntype = *type;
			if (parse_designator_list(&current_member, &noffset, &ntype)) {
				TEXPECT(T_A);
				current_member++;
			} else {
				type_select(*type, current_member++, &noffset, &ntype);
			}

			parse_initializer_recursive(offset + noffset, &ntype, 0, initializer);
			TACCEPT(T_COMMA);

			if (current_member > max_member)
				max_member = current_member;
		}

		if ((*type)->type == TY_INCOMPLETE_ARRAY && set_type) {
			struct type complete_array_params = {
				.type = TY_ARRAY,
				.array.length = max_member,
				.n = 1
			};

			struct type *ntype = type_create(&complete_array_params, (*type)->children);
			*type = ntype;
		}
	} else {
		struct expr *expr = parse_assignment_expression();
		if (!expr) {
			ERROR("Expected expression, got %s", token_to_str(T0->type));
		}

		if ((*type)->type == TY_INCOMPLETE_ARRAY && set_type) {
			if (expr->type != E_CONSTANT)
				ERROR("Can't initialize incomplete array with non constant expression");

			if (expr->data_type->type != TY_ARRAY)
				ERROR("Can't initialize incomplete array with non-array expression of type");

			if (expr->data_type->children[0] != (*type)->children[0])
				ERROR("Can't initialize incomplete array with array of incompatible type");

			struct constant c = expr->constant;

			struct type complete_array_params = {
				.type = TY_ARRAY,
				.array.length = c.data_type->array.length,
				.n = 1
			};

			struct type *ntype = type_create(&complete_array_params, (*type)->children);
			*type = ntype;

			add_init_pair(initializer, (struct init_pair){offset, expr});
		} else {
			add_init_pair(initializer, (struct init_pair){offset,
					expression_cast(expr, *type)});
		}

	}
}

struct initializer *parse_initializer(struct type **type) {
	struct initializer *init = initializer_init();

	parse_initializer_recursive(0, type, 1, init);

	return init;
}

struct type *parse_type_name(void) {
	struct specifiers s = {0};
	int has_spec = parse_specifiers(&s.ts, NULL, &s.tq, NULL, &s.as);

	if (!has_spec)
		return NULL;

	int was_abstract = 1;
	struct type_ast *ast = parse_declarator(&was_abstract);
	if (!was_abstract)
		ERROR("Type name must be abstract");

	return ast_to_type(&s.ts, ast, NULL);
}

int parse_init_declarator(struct specifiers s, int global, int *was_func) {
	*was_func = 0;
	(void)global;
	int was_abstract = 1;
	struct type_ast *ast = parse_declarator(&was_abstract);

	if (!ast)
		return 0;

	if (was_abstract) {
		PRINT_POS(TPEEK(0)->pos);
		printf("%s\n", token_to_str(TPEEK(0)->type));
		ERROR("Declaration can't be abstract");
	}

	struct type *type;
	char *name;
	type = ast_to_type(&s.ts, ast, &name);

	if (TPEEK(0)->type == T_LBRACE) {
		int arg_n = 0;
		char **arg_names = NULL;
		ast_get_parameters(ast, &arg_n, &arg_names);

		if (arg_n && !arg_names)
			ERROR("Should not be null");

		parse_function(name, type, arg_n, arg_names, s.scs.static_n ? 0 : 1);
		*was_func = 1;
		return 1;
	}

	if (s.scs.typedef_n) {
		symbols_add_typedef(name)->data_type = type;
		return 1;
	}

	// TODO: This system is not robust at all.
	// For example, defining variable twice in the same local
	// scope is allowed, even though it shouldn't.

	struct symbol_identifier *symbol = symbols_get_identifier_in_current_scope(name);

	struct type *prev_type = NULL;
	if (symbol) {
		if (symbol->type == IDENT_LABEL)
			prev_type = symbol->label.type;
		else if (symbol->type == IDENT_CONSTANT)
			prev_type = symbol->constant.data_type;
		else if (symbol->type == IDENT_VARIABLE)
			prev_type = symbol->variable.type;
		else
			NOTIMP();
	}

	if (!symbol)
		symbol = symbols_add_identifier(name);

	int definition = 1;
	int is_static = 0;
	int is_global = 0;

	// Create space for symbol if not a function prototype.
	if (global) {
		if (type->type == TY_FUNCTION) {
			symbol->type = IDENT_LABEL;
			symbol->label.name = name;
			symbol->label.type = type;
			return 1;
		} else {
			if (prev_type && type != prev_type) {
				type = prev_type;
			}

			is_static = 1;
			symbol->type = IDENT_LABEL;
			symbol->label.name = name;
			symbol->label.type = type;
			if (s.scs.extern_n) {
				definition = 0;
			}
			is_global = s.scs.static_n ? 0 : 1;
		}
	} else {
		if (s.scs.static_n)
			is_static = 1;
		else {
			symbol->type = IDENT_VARIABLE;
			if (has_variable_size(type)) {
				struct type *n_type = type;
				symbol->variable.id = allocate_vla(&n_type);
				symbol->variable.type = n_type;
				definition = 0;
			} else {
				symbol->variable.id = new_variable(type, 1);
				symbol->variable.type = type;
			}
		}
	}


	if (definition) {
		// Is variable that potentially has a initializer.
		struct type *prev_type = type;
		struct initializer *init = NULL;
		if (TACCEPT(T_A))
			init = parse_initializer(&type);

		if (is_static) {
			symbol->type = IDENT_LABEL;

			if (!global) {
				static int local_var = 0;
				name = allocate_printf(".LVAR%d%s", local_var++, name);
			}

			symbol->label.name = name;
			symbol->label.type = type;

			if (!s.scs.extern_n) {
				// init can be NULL
				data_register_static_var(name, type, init, is_global);
			}
		} else {
			// TODO: This doesn't feel very robust.
			if (prev_type != type) {
				change_variable_size(symbol->variable.id, calculate_size(type));
				symbol->variable.type = type;
			}

			if (init) {
				IR_PUSH_SET_ZERO(symbol->variable.id);

				for (int i = 0; i < init->n; i++) {
					IR_PUSH_ASSIGN_CONSTANT_OFFSET(symbol->variable.id, expression_to_ir(init->pairs[i].expr), init->pairs[i].offset);
				}
				symbol->variable = symbol->variable;
			}
		}
	}

	return 1;
}

int parse_declaration(int global) {
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
				ERROR("Invalid _Static_assert type: %s\n", type_to_string(constant->data_type));
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
	while (!was_func && parse_init_declarator(s, global, &was_func)) {
		TACCEPT(T_COMMA);
	}

	if (!was_func)
		TEXPECT(T_SEMI_COLON);

	return 1;
}

