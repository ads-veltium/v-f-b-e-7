#ifndef COMPONENTS_CPP_UTILS_CPPNVS_H_
#define COMPONENTS_CPP_UTILS_CPPNVS_H_
#include <esp_err.h>
#include <esp_log.h>

#include <string.h>
#include "nvs_flash.h"


/**
  	@file nvs32.h
	@author Jose Morais
	Class to use NVS.
	MIT License
	Copyright (c) 2020 José Morais
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
 */

class NVS
{
	private:
		const char tag[4] = "NVS";
		nvs_handle _handler;
		uint8_t _debug = 1;

	public:
		uint8_t init(const char *partition, const char *name, uint8_t debug);
		uint8_t erase_all();
		uint8_t erase_key(const char *key);

		uint8_t create(const char *key, const char *value);
		uint8_t create(const char *key, int32_t value);
		uint8_t create(const char *key, uint8_t value);

		uint8_t write(const char *key, const char *value);
		uint8_t write(const char *key, int32_t value);
		uint8_t write(const char *key, uint8_t value);

		uint8_t read(const char *key, char *dst, uint16_t size);
		uint8_t read(const char *key, int32_t *dst);
		uint8_t read(const char *key, uint8_t *dst);
        uint8_t close();
		
};

#endif