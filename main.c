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



static void dump_retbuf(const char *fmt, struct aura_buffer *buf)
{
	if (!fmt)
		return;

	while (*fmt) {
		switch (*fmt++) { 
		case URPC_U8: {
			uint8_t a = aura_buffer_get_u8(buf);
			printf(" %u ", a);
			break;
		}
 		case URPC_S8: {
			int8_t a = aura_buffer_get_s8(buf);
			printf(" %d ", a);
			break;
		}
		case URPC_U16: {
			int16_t a = aura_buffer_get_u16(buf);
			printf(" %u ", a);
			break;
		}
 		case URPC_S16: {
			int16_t a = aura_buffer_get_s16(buf);
			printf(" %d ", a);
			break;
		}
		case URPC_U32: {
			uint32_t a = aura_buffer_get_u32(buf);
			printf(" %u ", a);
			break;
		}

 		case URPC_S32: {
			int32_t a = aura_buffer_get_s32(buf);
			printf(" %d ", a);
			break;
		}

		case URPC_U64: {
			uint64_t a = aura_buffer_get_u64(buf);
			printf(" %lu ", a);
			break;
		}
			
 		case URPC_S64: {
			int64_t a = aura_buffer_get_s64(buf);
			printf(" %ld ", a);
			break;
		}

		case URPC_BUF:
			printf( " buf(0x%lx) ", aura_buffer_get_u64(buf));
			// TODO: Printout buffer here
			break;

		case URPC_BIN: {
			int tmp = atoi(fmt);
			if (tmp == 0) 
				BUG(NULL, "Internal serilizer bug processing: %s", fmt);
			while (*fmt && (*fmt++ != '.'));
			printf(" bin(%d) ", tmp);
			break;
		}

		default:
			BUG(NULL, "Serializer failed at token: %s", fmt); 
		}
	}
}


static void do_listen_for_events(struct aura_node *n)
{
	const struct aura_object *o; 
	struct aura_buffer *buf;
	int ret; 

	aura_enable_sync_events(n, 10); 

	while (1) { 		
		ret = aura_get_next_event(n, &o, &buf); 
		if (ret != 0)
			BUG(n, "Failed to read next event");
		printf("event: %s | ", o->name);
		dump_retbuf(o->ret_fmt, buf);
		printf("\n");
	}

}

static void do_call_method(struct aura_node *n, const char *name, const char *cmdlineopts)
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


