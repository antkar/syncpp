/*
 * Copyright 2014 Anton Karmanov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//Script Language interpreter entry point.

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gc.h"
#include "name.h"
#include "script.h"
#include "stringex__dec.h"
#include "value_core.h"

#include "syngen.h"

namespace ss = syn_script;
namespace rt = ss::rt;
namespace gc = ss::gc;

namespace {
	ss::StringLoc load_file(const std::string& file_name) {
		std::ifstream in;
		std::ostringstream out;

		try {
			in.exceptions(std::ios_base::badbit | std::ios_base::failbit);
			in.open(file_name, std::ios_base::in);
		} catch (const std::ios_base::failure&) {
			std::string msg = "File not found: ";
			msg += file_name;
			throw ss::RuntimeError(msg);
		}

		in.exceptions(std::ios_base::badbit);
		
		char c;
		while (in.get(c)) out << c;

		return gc::create<ss::String>(out.str());
	}

	gc::Local<ss::StringArray> create_arguments_array(const std::vector<std::string>& arguments_std) {
		std::size_t n = arguments_std.size();
		gc::Local<ss::StringArray> array = ss::StringArray::create(n);
		for (std::size_t i = 0; i < n; ++i) (*array)[i] = gc::create<ss::String>(arguments_std[i]);
		return array;
	}

	std::size_t get_effective_memory_limit(std::size_t limit_mb) {
		if (limit_mb == 0) limit_mb = 8;
		if (limit_mb > 2048) limit_mb = 2048;

		std::size_t limit = limit_mb << 20;
		try {
			char* test_data = new char[limit];
			free(test_data);
		} catch (std::bad_alloc) {
			throw ss::FatalError("Not enough memory");
		}

		return limit;
	}
}//namespace

void link__api();

int sample_main(
	const std::string& file_name_std,
	const std::vector<std::string>& arguments_std,
	std::size_t mem_limit_mb)
{
	link__api();

	try {
		gc::startup_guard gc_startup(get_effective_memory_limit(mem_limit_mb));
		gc::manage_thread_guard gc_thread;
		gc::enable_guard gc_enable;

		try {
			ss::StringLoc file_name = gc::create<ss::String>(file_name_std);
			ss::StringLoc code = load_file(file_name_std);
			gc::Local<ss::StringArray> arguments = create_arguments_array(arguments_std);
			gc::Local<gc::Array<rt::ScriptSource>> sources = rt::get_single_script_source(file_name, code);
			bool ok = rt::execute_top_script(sources, arguments);
			return ok ? 0 : 1;
		} catch (const ss::BasicError& e) {
			std::cerr << e << std::endl;
			return 1;
		} catch (const gc::out_of_memory&) {
			std::cerr << "Out of memory!\n";
			return 1;
		}
	} catch (const char* e) {
		std::cerr << "String exception: " << e << '\n';
		return 1;
	} catch (...) {
		std::cerr << "Unhandled exception!\n";
		return 1;
	}
}
