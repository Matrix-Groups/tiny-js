/*
 * TinyJS
 *
 * A single-file Javascript-alike engine
 *
 * Authored By Gordon Williams <gw@pur3.co.uk>
 *
 * Copyright (C) 2009 Pur3 Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

 /*
  * Profiler for TinyJS. Written by Alex Van Liew.
  */

#include "TinyJS.h"
#include "TinyJS_Functions.h"
#include "TinyJS_MathFunctions.h"
#include <time.h>	 
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>	 

using std::cout;
using std::endl;
using std::string;

void js_print(CScriptVar *v, void *userdata)
{
	printf("> %s\n", v->getParameter("text")->getString().c_str());
}

int main(int argc, char **argv)
{
	CTinyJS *js = new CTinyJS();
	/* add the functions from TinyJS_Functions.cpp */
	registerFunctions(js);
	registerFunctions(js);
	/* Add a native function */
	js->addNative("function print(text)", &js_print, 0);
	/* Execute out bit of code - we could call 'evaluate' here if
	   we wanted something returned */

	if(argc < 2)
	{
		if(argc > 0)
			printf("Usage: ./%s profile.js [NAME=VALUE...]\n", argv[0]);
		else
			printf("Usage: ./run_profiler profile.js [NAME=VALUE...]\n");
		printf("       profile.js: Name of file to run.\n");
		printf("       NAME=VALUE: Name/value pairs to override configuration values in the profiled file");
		return 1;
	}

	const char* filename = argv[1];
	struct stat results;
	if(!stat(filename, &results) == 0)
	{
		printf("Cannot stat file! '%s'\n", filename);
		return false;
	}
	int size = results.st_size;
	FILE *file = fopen(filename, "rb");
	/* if we open as text, the number of bytes read may be > the size we read */
	if(!file)
	{
		printf("Unable to open file! '%s'\n", filename);
		return false;
	}
	char *buffer = new char[size + 1];
	long actualRead = fread(buffer, 1, size, file);
	buffer[actualRead] = 0;
	buffer[size] = 0;
	fclose(file);

	try
	{
		// read in definitions from the buffer
		js->execute(buffer);

		// initialize default script parameters
		js->execute("init();");

		// set parameters from arguments
		for(int i = 2; i < argc; i++)
		{
			int idx = 0;
			while(argv[i][idx] != '=' and argv[i][idx] != '\0') idx++;
			if(argv[i][idx] == '\0') continue;

			argv[i][idx] = '=';
			idx++;
			// really, you might be fine just prepending "var " and appending ";" but
			// might as well be safe
			js->execute("var " + string(argv[i]) + "= (" + argv[i][idx] + ");");
		}

		// run algorithm setup
		js->execute("setup();");

		// start actual profiling
		double tstart, tstop, ttime;

		tstart = (double)clock() / CLOCKS_PER_SEC;
		string functionName = js->evaluate("run();");
		tstop = (double)clock() / CLOCKS_PER_SEC;
		ttime = tstop - tstart;

		// get iterations
		string iterations = js->evaluate("get_iterations();");

		cout << "Profiled function " + functionName + "." << endl;
		cout << "Ran " + functionName + " a total of " + iterations + " time(s)." << endl;
		cout << "Total time elapsed: " << ttime << ". Time per execution: " << ttime / atoi(iterations.c_str()) << endl;
	}  
	catch(CScriptException *e)
	{
		printf("ERROR: %s\n", e->text.c_str());
	}

	delete js;
#ifdef _WIN32
#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
#endif
	return 0;
}
