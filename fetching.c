#include "state.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json/json.h"
#include "config.h"

struct string {
	char *ptr;
	size_t len;
};

void init_string(struct string *s)
{
	s->len = 0;
	s->ptr = malloc(s->len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
	size_t new_len = s->len + size * nmemb;
	s->ptr = realloc(s->ptr, new_len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	memcpy(s->ptr + s->len, ptr, size * nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size * nmemb;
}
#define ROOT json->u.array
#define CUR_OBJ ROOT.values[i]->u.object

int parse_examples(struct example e[], json_value *arr[], int n)
{
	for (int i = 0; i < n; i++) {
		json_object_entry *v = arr[i]->u.object.values;
		short vc = arr[i]->u.object.length;
		for (int j = 0; j < vc; j++) {
			if (!strncmp(v[j].name, "src", v[j].name_length)) {
				*(v[j].value->u.string.ptr +
				  v[j].value->u.string.length) = 0;
				e[i].src = v[j].value->u.string.ptr;
			}
			if (!strncmp(v[j].name, "dst", v[j].name_length)) {
				*(v[j].value->u.string.ptr +
				  v[j].value->u.string.length) = 0;
				e[i].dst = v[j].value->u.string.ptr;
			}
		}
	}
	return 0;
}

int parse_translations(struct translation t[], json_value *arr[], int n)
{
	for (int i = 0; i < n; i++) {
		json_object_entry *v = arr[i]->u.object.values;
		short vc = arr[i]->u.object.length;
		for (int j = 0; j < vc; j++) {
			if (!strncmp(v[j].name, "text", v[j].name_length)) {
				*(v[j].value->u.string.ptr +
				  v[j].value->u.string.length) = 0;
				t[i].text = v[j].value->u.string.ptr;
			}
			if (!strncmp(v[j].name, "pos", v[j].name_length)) {
				*(v[j].value->u.string.ptr +
				  v[j].value->u.string.length) = 0;
				t[i].pos = v[j].value->u.string.ptr;
			}
			if (!strncmp(v[j].name, "examples", v[j].name_length)) {
				t[i].examples_num = v[j].value->u.array.length;
				parse_examples(t[i].examples,
					       v[j].value->u.array.values,
					       t[i].examples_num);
			}
		}
	}
	return 0;
}

int get_response(CURL *curl, struct client_state *state)
{
	struct string s;
	init_string(&s);
	char formatted_query[100];

	CURLcode res;
        char *user_query = curl_easy_escape(curl, state->query.text, strlen(state->query.text));
	sprintf(formatted_query,
		SERVER_URL"/api/v2/translations?query=%s&src=de&dst=en&guess_direction=true",
		user_query);

        curl_free(user_query);

	curl_easy_setopt(curl, CURLOPT_URL, formatted_query);
	/* example.com is redirected, so we tell libcurl to follow redirection */
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

	/* Perform the request, res gets the return code */

	res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
			curl_easy_strerror(res));
                return 1;
        }
	if (http_code != 200) {
		fprintf(stderr, "server error: %s\n", s.ptr);
                return 1;
	}

	// THIS IS A BRILLIANT IDEA! At the end of s.ptr place a null byte
	json_value *json = json_parse(s.ptr, s.len);
	if (json->type != json_array) {
		state->results = NULL;
		return 0;
	}
	state->results_num = ROOT.length;
        // NULL will indicate that we have already allocated something before
        if (state->results != NULL) {
                free(state->results);
                state->results = NULL;
        }
	state->results = malloc(sizeof(struct result) * ROOT.length);
	for (int i = 0; i < ROOT.length; i++) {
		for (int j = 0; j < CUR_OBJ.length; j++) {
			if (!strncmp(CUR_OBJ.values[j].name, "text",
				     CUR_OBJ.values[j].name_length)) {
				*(CUR_OBJ.values[j].value->u.string.ptr +
				  CUR_OBJ.values[j].value->u.string.length) =
					0; // convert it to null byteable string
				(*state->results)[i].text =
					CUR_OBJ.values[j].value->u.string.ptr;
			}
			if (!strncmp(CUR_OBJ.values[j].name, "pos",
				     CUR_OBJ.values[j].name_length)) {
				*(CUR_OBJ.values[j].value->u.string.ptr +
				  CUR_OBJ.values[j].value->u.string.length) =
					0; // convert it to null byteable string
				(*state->results)[i].pos =
					CUR_OBJ.values[j].value->u.string.ptr;
			}
			if (!strncmp(CUR_OBJ.values[j].name, "translations",
				     CUR_OBJ.values[j].name_length)) {
				(*state->results)[i].translations_num =
					CUR_OBJ.values[j].value->u.array.length;
				parse_translations(
					(*state->results)[i].translations,
					CUR_OBJ.values[j].value->u.array.values,
					CUR_OBJ.values[j].value->u.array.length);
			}
		}
	}

	free(s.ptr);

	return 0;
}
