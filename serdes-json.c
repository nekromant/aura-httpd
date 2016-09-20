#include <aura/aura.h>
#include <aura/private.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/vfs.h>


json_object *ahttpd_format_to_json(const char *fmt)
{
	int len;
	struct json_object *ret = json_object_new_array();

	if (!fmt)
		return ret;

	while (*fmt) {
		struct json_object *tmp;
		switch (*fmt++) {
		case URPC_U8:
			tmp = json_object_new_string("uint8_t");
			break;
		case URPC_U16:
			tmp = json_object_new_string("uint16_t");
			break;
		case URPC_U32:
			tmp = json_object_new_string("uint32_t");
			break;
		case URPC_U64:
			tmp = json_object_new_string("uint64_t");
			break;

		case URPC_S8:
			tmp = json_object_new_string("int8_t");
			break;
		case URPC_S16:
			tmp = json_object_new_string("int16_t");
			break;
		case URPC_S32:
			tmp = json_object_new_string("int32_t");
			break;
		case URPC_S64:
			tmp = json_object_new_string("int64_t");
			break;
		case URPC_BUF:
			tmp = json_object_new_string("buffer");
			break;
		case URPC_BIN:
			len = atoi(fmt);
			tmp = json_object_new_object();
			struct json_object *ln = NULL;
			if (len == 0)
				BUG(NULL, "Internal serializer bug processing: %s", fmt);
			else
				ln = json_object_new_int(len);
			if (!ln || !tmp)
				BUG(NULL, "Out of memory while serializing json");
			json_object_object_add(tmp, "binary", ln);
			while (*fmt && (*fmt++ != '.'));
			break;
		default:
				BUG(NULL, "Invalid format string");
			break;
		}
		json_object_array_add(ret, tmp);
	}
	return ret;
}

json_object *ahttpd_buffer_to_json(struct aura_buffer *buf, const char *fmt)
{
	struct json_object *ret = json_object_new_array();

	if (!ret)
		return NULL;

		union {
			uint8_t u8;
			uint16_t u16;
			uint32_t u32;
			uint64_t u64;
			int8_t	s8;
			int16_t	s16;
			int32_t	s32;
			int64_t	s64;
		} var;

#define FETCH_VAR_IN_CASE(cs, sz) \
case cs: \
	var.sz = aura_buffer_get_ ## sz(buf); \
	tmp = json_object_new_int64(var.sz); \
	break;

	if (!fmt)
		return ret;
	while (*fmt) {
		int len;
		struct json_object *tmp = NULL;
		switch (*fmt++) {
			FETCH_VAR_IN_CASE(URPC_U8, u8);
			FETCH_VAR_IN_CASE(URPC_S8, s8);
			FETCH_VAR_IN_CASE(URPC_U16, u16);
			FETCH_VAR_IN_CASE(URPC_S16, s16);
			FETCH_VAR_IN_CASE(URPC_U32, u32);
			FETCH_VAR_IN_CASE(URPC_S32, s32);
			FETCH_VAR_IN_CASE(URPC_U64, u64);
			FETCH_VAR_IN_CASE(URPC_S64, s64);

		case URPC_BUF:
			//FixMe: TODO:...
			tmp = json_object_new_string("buffer");
			break;
		case URPC_BIN:
			len = atoi(fmt);
			tmp = json_object_new_string("buffer");
			slog(0, SLOG_WARN, "Ignoring %d byte buffer - not implemented", len);
			while (*fmt && (*fmt++ != '.'));
			break;
		default:
			BUG(NULL, "Invalid format string");
			break;
		}

		if (!tmp)
			BUG(NULL, "WTF?");
		json_object_array_add(ret, tmp);
		tmp = NULL;
	}
	return ret;
}


static int json_object_is_type_log(struct json_object *jo, enum json_type want)
{
	enum json_type tp;
	tp = json_object_get_type(jo);
	if (tp != want) {
		slog(0, SLOG_WARN, "Bad JSON data. Want %s got %s",
		json_type_to_name(want),
		json_type_to_name(tp));
		return 0;
	}
	return 1;
}

int ahttpd_buffer_from_json(struct aura_buffer *	buf,
		     struct json_object *	json,
		     const char *		fmt)
{
	int i = 0;

	if (!json_object_is_type_log(json, json_type_array))
		return -1;

	if (!fmt)
		return 0; /* All done ;) */

	union {
		uint8_t u8;
		uint16_t u16;
		uint32_t u32;
		uint64_t u64;
		int8_t	s8;
		int16_t	s16;
		int32_t	s32;
		int64_t	s64;
	} var;

#define PUT_VAR_IN_CASE(cs, sz) \
case cs: \
	tmp = json_object_array_get_idx(json, i++); \
	if (!json_object_is_type_log(tmp, json_type_int)) {\
		return -1; \
	} \
	var.sz = json_object_get_int64(tmp); \
	aura_buffer_put_ ## sz(buf, var.sz); \
	break;

	while (*fmt) {
		int len;
		struct json_object *tmp = NULL;
		switch (*fmt++) {
			PUT_VAR_IN_CASE(URPC_U8, u8);
			PUT_VAR_IN_CASE(URPC_S8, s8);
			PUT_VAR_IN_CASE(URPC_U16, u16);
			PUT_VAR_IN_CASE(URPC_S16, s16);
			PUT_VAR_IN_CASE(URPC_U32, u32);
			PUT_VAR_IN_CASE(URPC_S32, s32);
			PUT_VAR_IN_CASE(URPC_U64, u64);
			PUT_VAR_IN_CASE(URPC_S64, s64);

		case URPC_BUF:
			BUG(NULL, "Not implemented");
			break;
		case URPC_BIN:
			len = atoi(fmt);
			tmp = json_object_array_get_idx(json, i++); \
			if (json_object_is_type(tmp, json_type_int)) {
				/* TODO: Fetch from handle, binary upload */
			} else if (json_object_is_type(tmp, json_type_string)) {
				/* Just copy the json string */
				const char *str = json_object_to_json_string(tmp);
				int tocopy = min_t(int, strlen(str), len);
				aura_buffer_put_bin(buf, str, tocopy);
			}
			while (*fmt && (*fmt++ != '.'));
			break;
		case 0x0:
			BUG(NULL, "Not implemented");
			break;
		}
	}
	return 0;
}
