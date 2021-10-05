#ifndef UNARY_OPERATORS_H
#define UNARY_OPERATORS_H
#include <parser/parser.h>

const char *unary_operator_outputs[OT_TYPE_COUNT][UOP_TYPE_COUNT] = {                                      
	[OT_INT][UOP_PLUS] = "movl\t%edi, %eax\n",
	[OT_INT][UOP_NEG] = "movl\t%edi, %eax\n\tnegl\t%eax\n",
	[OT_INT][UOP_NOT] = "xorl\t%eax, %eax\n\ttestl\t%edi, %edi\n\tsete\t%al\n",
	[OT_INT][UOP_BNOT] = "movl\t%edi, %eax\n\tnotl\t%eax\n",
	[OT_UINT][UOP_PLUS] = "movl\t%edi, %eax\n",
	[OT_UINT][UOP_NEG] = "movl\t%edi, %eax\n\tnegl\t%eax\n",
	[OT_UINT][UOP_NOT] = "xorl\t%eax, %eax\n\ttestl\t%edi, %edi\n\tsete\t%al\n",
	[OT_UINT][UOP_BNOT] = "movl\t%edi, %eax\n\tnotl\t%eax\n",
	[OT_LONG][UOP_PLUS] = "movq\t%rdi, %rax\n",
	[OT_LONG][UOP_NEG] = "movq\t%rdi, %rax\n\tnegq\t%rax\n",
	[OT_LONG][UOP_NOT] = "xorl\t%eax, %eax\n\ttestq\t%rdi, %rdi\n\tsete\t%al\n",
	[OT_LONG][UOP_BNOT] = "movq\t%rdi, %rax\n\tnotq\t%rax\n",
	[OT_ULONG][UOP_PLUS] = "movl\t%edi, %eax\n",
	[OT_ULONG][UOP_NEG] = "movl\t%edi, %eax\n\tnegl\t%eax\n",
	[OT_ULONG][UOP_NOT] = "xorl\t%eax, %eax\n\ttestl\t%edi, %edi\n\tsete\t%al\n",
	[OT_ULONG][UOP_BNOT] = "movl\t%edi, %eax\n\tnotl\t%eax\n",
	[OT_LLONG][UOP_PLUS] = "movq\t%rdi, %rax\n",
	[OT_LLONG][UOP_NEG] = "movq\t%rdi, %rax\n\tnegq\t%rax\n",
	[OT_LLONG][UOP_NOT] = "xorl\t%eax, %eax\n\ttestq\t%rdi, %rdi\n\tsete\t%al\n",
	[OT_LLONG][UOP_BNOT] = "movq\t%rdi, %rax\n\tnotq\t%rax\n",
	[OT_ULLONG][UOP_PLUS] = "movq\t%rdi, %rax\n",
	[OT_ULLONG][UOP_NEG] = "movq\t%rdi, %rax\n\tnegq\t%rax\n",
	[OT_ULLONG][UOP_NOT] = "xorl\t%eax, %eax\n\ttestq\t%rdi, %rdi\n\tsete\t%al\n",
	[OT_ULLONG][UOP_BNOT] = "movq\t%rdi, %rax\n\tnotq\t%rax\n",
};

#endif
