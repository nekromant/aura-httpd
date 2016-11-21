#include <stdbool.h>
#include <string.h>
#include <json.h>
#include <aura-httpd/json.h>

struct json_object *json_array_find(json_object *arr, char *k)
{
	json_object_object_foreach(arr, key, val) {
		if (strcmp(key, k) == 0)
			return val;
	}
	return NULL;
}

const char *json_array_find_string(json_object *o, char *k)
{
	json_object *tmp = json_array_find(o, k);
	if (!tmp)
		return NULL;
	return json_object_get_string(tmp);
}

long json_array_find_number(json_object *o, char *k)
{
	json_object *tmp = json_array_find(o, k);
	if (!tmp)
		return -1;
	return json_object_get_int64(tmp);
}

bool json_array_find_boolean(json_object *o, char *k)
{
	json_object *tmp = json_array_find(o, k);
	if (!tmp)
		return false;
	int ret = json_object_get_boolean(tmp);
	return ret;
}
