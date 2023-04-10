// Copyright (c) 2023 Dark Energy Processor
//
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
// DEALINGS IN THE SOFTWARE.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <cstdint>
#include <cstdio>

#include <list>
#include <unordered_map>
#include <stack>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "JSonParser/api/yajl_gen.h"
#include "JSonParser/api/yajl_parse.h"

enum json_type {
	json_type_null,
	json_type_boolean,
	json_type_integer,
	json_type_real,
	json_type_string,
	json_type_map,
	json_type_array
};

struct json_data
{
	json_type type;

	union {
		bool boolean;
		int64_t integer;
		double real;
		std::string string;
		std::unordered_map<std::string, json_data> map;
		std::list<json_data> array;
	};

	json_data(json_type t)
	: type(t)
	{
		switch (t)
		{
			default:
			case json_type_null:
				break;
			case json_type_boolean:
				boolean = false;
				break;
			case json_type_integer:
				integer = 0;
				break;
			case json_type_real:
				real = 0.0;
				break;
			case json_type_string:
				new (&string) std::string();
				break;
			case json_type_map:
				new (&map) std::unordered_map<std::string, json_data>();
				break;
			case json_type_array:
				new (&array) std::list<json_data>();
				break;
		}
	}
	json_data(bool b)
	: type(json_type_boolean)
	, boolean(b)
	{ }
	json_data(int64_t i)
	: type(json_type_integer)
	, integer(i)
	{ }
	json_data(double r)
	: type(json_type_real)
	, real(r)
	{ }
	json_data(const char *str, size_t size)
	: type(json_type_string)
	{
		new (&string) std::string(str, size);
	}
	~json_data() {
		switch (type)
		{
			default:
			case json_type_null:
			case json_type_boolean:
			case json_type_integer:
			case json_type_real:
				break;
			case json_type_string:
				string.~basic_string();
				break;
			case json_type_map:
				map.~unordered_map();
				break;
			case json_type_array:
				array.~list();
				break;
		}
	}
};

struct json_data_state
{
	std::stack<json_data*> stack;
	std::string tempKey;
	json_data *root;

	template<class... Args>
	json_data *insert(Args&&... args)
	{
		if (root == nullptr)
		{
			root = new json_data(std::forward<Args>(args)...);
			return root;
		}
		else if (stack.empty())
			return nullptr;
			
		json_data *current = stack.top();
		if (current->type == json_type_array)
		{
			current->array.emplace_back(std::forward<Args>(args)...);
			return &current->array.back();
		}
		else if (current->type == json_type_map)
		{
			//current->map.emplace(tempKey, std::forward<Args>(args)...);
			current->map.emplace(
				std::piecewise_construct,
				std::forward_as_tuple(tempKey),
				std::forward_as_tuple(args...)
			);
			return &current->map.at(tempKey);
		}

		return nullptr;
	}
};

static yajl_gen_status generate_json(yajl_gen gen, const json_data *root)
{
	yajl_gen_status status;

	switch (root->type)
	{
		default:
		case json_type_null:
			return yajl_gen_null(gen);
		case json_type_boolean:
			return yajl_gen_bool(gen, (int) root->boolean);
		case json_type_integer:
			return yajl_gen_integer(gen, root->integer);
		case json_type_real:
			return yajl_gen_double(gen, root->real);
		case json_type_string:
			return yajl_gen_string(gen, (const unsigned char *) root->string.data(), root->string.length());
		case json_type_map:
			status = yajl_gen_map_open(gen);
			if (status != yajl_gen_status_ok)
				return status;

			for (const auto &item: root->map)
			{
				status = yajl_gen_string(gen, (const unsigned char *) item.first.data(), item.first.length());
				if (status != yajl_gen_status_ok)
					return status;

				status = generate_json(gen, &item.second);
				if (status != yajl_gen_status_ok)
					return status;
			}

			return yajl_gen_map_close(gen);
		case json_type_array:
			status = yajl_gen_array_open(gen);
			if (status != yajl_gen_status_ok)
				return status;

			for (const auto &item: root->array)
			{
				status = generate_json(gen, &item);
				if (status != yajl_gen_status_ok)
					return status;
			}

			return yajl_gen_array_close(gen);
	}
}

static void usage(char *arg0)
{
	printf("Usage: %s <input> [output=input]\nInput and output can be \"-\" for stdin and stdout respectively\n", arg0);
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		usage(argv[0]);
		return 1;
	}

#ifdef _WIN32
	_setmode(fileno(stdin), O_BINARY);
#endif

	FILE *f = strcmp(argv[1], "-") == 0 ? stdin : fopen(argv[1], "rb");
	if (f == nullptr)
	{
		perror(argv[1]);
		return 1;
	}

	static yajl_callbacks reader_cb = {
		// read null
		[](void *ctx)
		{
			json_data_state *state = (json_data_state *) ctx;
			return (int) (state->insert(json_type_null) != nullptr);
		},
		// read boolean
		[](void *ctx, int boolean)
		{
			json_data_state *state = (json_data_state *) ctx;
			return (int) (state->insert((bool) boolean) != nullptr);
		},
		// read integer
		[](void *ctx, long long integer)
		{
			json_data_state *state = (json_data_state *) ctx;
			return (int) (state->insert(integer) != nullptr);
		},
		// read double
        [](void *ctx, double real)
		{
			json_data_state *state = (json_data_state *) ctx;
			return (int) (state->insert(real) != nullptr);
		},
        nullptr,
		// read string
        [](void *ctx, const unsigned char *stringVal, size_t stringLen, int cte_pool)
		{
			json_data_state *state = (json_data_state *) ctx;
			return (int) (state->insert((const char *) stringVal, stringLen) != nullptr);
		},
		// start map
        [](void *ctx, unsigned int size)
		{
			json_data_state *state = (json_data_state *) ctx;
			json_data *jdata = state->insert(json_type_map);

			if (jdata == nullptr)
				return 0;
			
			state->stack.push(jdata);
			return 1;
		},
		// map key
        [](void *ctx, const unsigned char * key, size_t stringLen, int cte_pool)
		{
			json_data_state *state = (json_data_state *) ctx;
			state->tempKey = std::string((const char *) key, stringLen);
			return 1;
		},
		// end map
        [](void *ctx)
		{
			json_data_state *state = (json_data_state *) ctx;
			state->stack.pop();
			return 1;
		},
		// start array
        [](void *ctx, unsigned int size)
		{
			json_data_state *state = (json_data_state *) ctx;
			json_data *jdata = state->insert(json_type_array);

			if (jdata == nullptr)
				return 0;
			
			state->stack.push(jdata);
			return 1;
		},
		// end array
        [](void *ctx)
		{
			json_data_state *state = (json_data_state *) ctx;
			state->stack.pop();
			return 1;
		}
	};

	json_data_state state = {{}, "", nullptr};
	yajl_handle hand = yajl_alloc(&reader_cb, nullptr, &state);
	yajl_config(hand, yajl_allow_comments, 1); 

	constexpr size_t READ_BUFSIZE = 4096;
	std::vector<unsigned char> buf(READ_BUFSIZE);
	size_t readed = READ_BUFSIZE;
	size_t fullSize = 0;
	yajl_status status = yajl_status_ok;

	while (readed == READ_BUFSIZE)
	{
		readed = fread(buf.data() + fullSize, 1, READ_BUFSIZE, f);
		fullSize += readed;
		buf.resize(buf.capacity() + READ_BUFSIZE);
	}

	if (f != stdin)
    	fclose(f);

	yajl_parse(hand, buf.data(), fullSize);
	status = yajl_complete_parse(hand);
	if (status != yajl_status_ok)
	{
		unsigned char *str = yajl_get_error(hand, 1, buf.data(), readed);  
        fprintf(stderr, "%s: %s\n", argv[1], (const char *) str);  
        yajl_free_error(hand, str);  
		yajl_free(hand);
		return 1;
	}

	yajl_free(hand);

	const char *output = argc < 3 ? argv[1] : argv[2];
	f = strcmp(output, "-") == 0 ? stdout : fopen(output, "wb");
	if (f == NULL)
	{
		perror(argv[2]);
		return 1;
	}

	yajl_gen gen = yajl_gen_alloc(nullptr);
	yajl_gen_config(gen, yajl_gen_beautify, 1);
	generate_json(gen, state.root);

	const unsigned char *genbuf;
	size_t gensize;
	yajl_gen_get_buf(gen, &genbuf, &gensize);
	fwrite(genbuf, 1, gensize, f);
	yajl_gen_free(gen);

	if (f != stdout)
		fclose(f);

	return 0;
}
