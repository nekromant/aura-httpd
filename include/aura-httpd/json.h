#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H

struct json_object *json_array_find(json_object *arr, char *k);
const char *json_array_find_string(json_object *o, char *k);
long json_array_find_number(json_object *o, char *k);
bool json_array_find_boolean(json_object *o, char *k);


#endif /* end of include guard: JSON_HELPERS_H */
