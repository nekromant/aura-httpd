#include <aura/aura.h>
#include <getopt.h>
#include <aura/private.h>

enum opcode { 
	OP_NONE,
	OP_LISTEN,
	OP_CALL,
};

static int op;
static struct option long_options[] =
{
	/* These options set a flag. */
//	{"version", no_argument,       &verbose_flag, 1},
	/* These options don’t set a flag.
	   We distinguish them by their indices. */
	{"list-transports",   no_argument,             0, 'L'  },
	{"version",           no_argument,             0, 'v'  },
	{"transport",         required_argument,       0, 't'  },
	{"option",            required_argument,       0, 'o'  },
	{"listen",            no_argument,             0, 'l'   },
	{"call",              required_argument,       0, 'c'  },
	{"log-level",         required_argument,       0, 'z'  },
	{0, 0, 0, 0}
};

void usage(char *self) { 
	
}


void do_listen_for_events(struct aura_node *n)
{
	
}

void do_call_method(struct aura_node *n, const char *name, const char *cmdlineopts)
{
	
}


int main(int argc, char *argv[]) 
{
	slog_init(NULL, 0);

	int option_index = 0;

	const char *name  = NULL; 
	const char *tname = NULL;
	const char *topts = NULL;

	while (1) { 
		int c = getopt_long (argc, argv, "Llvt:c:z:o:",
				     long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
		case 'L':
			aura_transport_dump_usage();
			exit(0);
			break;
		case 'l':
			op = OP_LISTEN;
			break;

		case 'v':
			printf("AURA Commandline Tool. Version 0.1 Using libaura (c) Necromant 2015\n");
			exit(0);
			break;
		case 'z':
			slog_init(NULL, atoi(optarg));
			break;
		case 'c':
			name = optarg;
			op = OP_CALL;
			break;
		case 'o':
			topts = optarg;
			break;
		case 't':
			tname = optarg;
			break;

		default:
			abort ();
		}
	}
	
	if (!tname) { 
		slog(0, SLOG_ERROR, "Can't do anything without a transport name");
		exit(1);
	}

	slog(0, SLOG_INFO, "start!");
	struct aura_node *n = aura_open(tname, topts);
	if (!n) 
		exit(1);

	switch (op) { 
	case OP_LISTEN:
		do_listen_for_events(n);
		break;
	case OP_CALL:
		do_call_method(n, name, NULL);
	}
	aura_close(n);
	return 0;
}


