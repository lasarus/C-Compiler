#ifndef UNARY_OPERATORS_H
#define UNARY_OPERATORS_H
#include <parser/parser.h>

const char *unary_operator_outputs[OT_TYPE_COUNT][UOP_TYPE_COUNT] = {
	[OT_INT][UOP_PLUS] = "movl\t%edi, %eax",
	[OT_INT][UOP_NEG] = "movl\t%edi, %eax\n\tnegl\t%eax",
	[OT_INT][UOP_BNOT] = "movl\t%edi, %eax\n\tnotl\t%eax",
	[OT_UINT][UOP_PLUS] = "movl\t%edi, %eax",
	[OT_UINT][UOP_NEG] = "movl\t%edi, %eax\n\tnegl\t%eax",
	[OT_UINT][UOP_BNOT] = "movl\t%edi, %eax\n\tnotl\t%eax",
	[OT_LONG][UOP_PLUS] = "movq\t%rdi, %rax",
	[OT_LONG][UOP_NEG] = "movq\t%rdi, %rax\n\tnegq\t%rax",
	[OT_LONG][UOP_BNOT] = "movq\t%rdi, %rax\n\tnotq\t%rax",
	[OT_ULONG][UOP_PLUS] = "movq\t%rdi, %rax",
	[OT_ULONG][UOP_NEG] = "movq\t%rdi, %rax\n\tnegq\t%rax",
	[OT_ULONG][UOP_BNOT] = "movq\t%rdi, %rax\n\tnotq\t%rax",
	[OT_LLONG][UOP_PLUS] = "movq\t%rdi, %rax",
	[OT_LLONG][UOP_NEG] = "movq\t%rdi, %rax\n\tnegq\t%rax",
	[OT_LLONG][UOP_BNOT] = "movq\t%rdi, %rax\n\tnotq\t%rax",
	[OT_ULLONG][UOP_PLUS] = "movq\t%rdi, %rax",
	[OT_ULLONG][UOP_NEG] = "movq\t%rdi, %rax\n\tnegq\t%rax",
	[OT_ULLONG][UOP_BNOT] = "movq\t%rdi, %rax\n\tnotq\t%rax",
	[OT_FLOAT][UOP_PLUS] = "movl\t%edi, %eax",
	[OT_FLOAT][UOP_NEG] = "movd\t%edi, %xmm0\n\tmovd\t%xmm0, %eax",
	//[OT_FLOAT][UOP_BNOT] = "",
	[OT_DOUBLE][UOP_PLUS] = "movq\t%rdi, %rax",
	[OT_DOUBLE][UOP_NEG] = "movq\t%rdi, %xmm0\n\tmovq\t%xmm0, %rax",
	//[OT_DOUBLE][UOP_BNOT] = "",
};

#endif
