// Default compiler settings.

static const char *default_include[] = {
	"include/linux/",
	"/usr/include/",
	NULL
};

static const char *default_defs[] = {
	"__linux",
	NULL
};

static enum {
	ABI_SYSV,
	ABI_MICROSOFT
} abi = ABI_SYSV;

static int mingw_workarounds = 0;
