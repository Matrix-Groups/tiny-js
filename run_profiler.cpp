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
#include <float.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>	 
#include <sstream>
#include <cstring>

#ifdef _MSC_VER
#include <Psapi.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif

using std::cout;
using std::endl;
using std::string;

void js_print(CScriptVar *v, void *userdata)
{
    printf("> %s\n", v->getParameter("text")->getString().c_str());
}

int usage(const char* name)
{
	printf("Usage: ./%s [--jit n] profile.js [NAME=VALUE...]\n", name);
	printf("       --jit n: Set the JIT compilation to occur after n executions. Default is 1.\n");
	printf("                Setting n=0 will disable compilation.");
	printf("                (This argument must appear here or nowhere.)");
	printf("       profile.js: Name of file to run.\n");
	printf("       NAME=VALUE: Name/value pairs to override configuration values in the profiled file");
	printf("\n");
	printf("       Utility for profiling the JIT-compiling function of the tiny-js\n");
	printf("       library. Profiles times per execution pre-JIT, post-JIT, and time to\n");
	printf("       compile. Also profiles memory pressure in the form of max resident set\n"); 
	printf("       for pre-compile and post-compile executions; this profiling includes\n");
	printf("       attempts to screen out startup memory costs so that displayed values are\n");
	printf("       exclusively representative of memory pressure due to JITted code. However,\n");
	printf("       on some systems, a large chunk of memory is allocated during startup and\n");
	printf("       then freed, causing the resident set to be artifically inflated and shadow\n");
	printf("       the memory used by the actual program. These cases are detected and noted by\n");
	printf("       the profiler.\n");
	return 1;
}

bool getmemusage(int& usage)
{
#ifdef _MSC_VER
	PROCESS_MEMORY_COUNTERS pmc;
	if(!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
	{
		TRACE("Failed in GetProcessMemoryInfo(), memory profiling disabled.\n");
		return false;
	}
	else
		usage = pmc.PeakWorkingSetSize / 1024;
#else
	struct rusage memusage;
	if(getrusage(RUSAGE_SELF, &memusage))
	{
		TRACE("Failed in getrusage(), memory profiling disabled.\n");
		return false;
	}
	else
		usage = memusage.ru_maxrss;
#endif
	return true;
}

int main(int argc, char **argv)
{
	if(argc < 2)
		return usage(argc ? argv[0] : "run_profiler");

	char* fname;
	int jit_at = 1;
	int i = 2;
	if(!strcmp(argv[1], "--jit"))
	{
		if(argc < 4)
			return usage(argv[0]);

		std::stringstream st(argv[2]);
		st >> jit_at;
		if(!st)
		{
			printf("Argument to --jit was not an int.");
			return usage(argv[0]);
		}
		fname = argv[3];
		i = 4;
	}
	else
		fname = argv[1];

	/* Create the interpreter with the specified number of executions */
	CTinyJS *js = new CTinyJS(jit_at);
	/* add the functions from TinyJS_Functions.cpp */
	registerFunctions(js);
	registerMathFunctions(js);
	/* Add a native function (this particular one isn't used in our scripts anymore, though) */
	js->addNative("function print(text)", &js_print, 0);
	/* Execute out bit of code - we could call 'evaluate' here if
	we wanted something returned */

    const char* filename = fname;
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
		// i is already set when arguments are parsed above
		for(; i < argc; i++)
		{
			int idx = 0;
			while(argv[i][idx] != '=' && argv[i][idx] != '\0') idx++;
			if(argv[i][idx] == '\0') continue;

			argv[i][idx++] = '\0';
			// really, you might be fine just prepending "var " and appending ";" but
			// might as well be safe
			js->execute("var " + string(argv[i]) + "= (" + &(argv[i][idx]) + ");");
		}

		// get iterations
		CScriptVarLink iter = js->evaluateComplex("get_iterations();");
		int iterations = iter.var->getInt();

		// get function
		string fnname = js->evaluate("get_function_name();");
		string profstring = fnname + "(" + js->evaluate("get_arg_list();") + ");";

		// run algorithm setup
		js->execute("setup();");
		
		// setup memory profiling
		bool memprof = false;
		int prestart_wss;
		int postmemoryusage;
		int prememoryusage;

		// start actual profiling
		double tstart, tstop;
		double* times = new double[iterations];
		printf("Beginning profiling, please be patient!\n");

		memprof = getmemusage(prestart_wss);
		for(i = 0; i < iterations; i++)
		{
			if(i == jit_at && memprof)
			{
				int precompile_usage;
				prememoryusage = (memprof = getmemusage(precompile_usage)) ? precompile_usage - prestart_wss : 0;
				if(memprof && !prememoryusage)
					prememoryusage = prestart_wss;
			}

			tstart = (double)clock() / CLOCKS_PER_SEC;
			js->evaluate(profstring);
			tstop = (double)clock() / CLOCKS_PER_SEC;
			times[i] = tstop - tstart;
		}

		// calculate max/min for precompile/postcompile, avg for pre/post
		double premax = 0, premin = DBL_MAX, preavg = 0;
		double postmax = 0, postmin = DBL_MAX, postavgincl = 0, postavgexcl = 0;
		double compileexec = 0;
		double avg = 0;
		for(i = 0; i < jit_at; i++)
		{
			double comp = times[i];
			if(premax < comp)
				premax = comp;
			if(premin > comp)
				premin = comp;
			preavg += comp;
		}
		avg += preavg;
		preavg /= jit_at;
		for(; i < iterations; i++)
		{
			double comp = times[i];
			if(i == jit_at)
				compileexec = comp;
			if(postmax < comp && i != jit_at)
				postmax = comp;
			if(postmin > comp)
				postmin = comp;
			postavgincl += comp;
		}
		// there was only one execution, the compilation one
		if(postmax == DBL_MAX)
			postmax = compileexec;
		avg += postavgincl;
		postavgexcl = postavgincl - compileexec;
		postavgexcl /= iterations - jit_at;
		postavgincl /= iterations - jit_at;
		avg /= iterations;

		if(memprof)
		{
			int finish_wss;
			postmemoryusage = (memprof = getmemusage(finish_wss)) ? finish_wss - prestart_wss : 0;
			if(memprof && !postmemoryusage)
				postmemoryusage = prestart_wss;
		}

        cout << "Profiled function " << fnname << " a total of " + iter.var->getString() + " time(s)." << endl;
		cout << "Pre-compile min, max, average: " << premin << "s, " << premax << "s, " << preavg << "s" << endl;
		cout << "Execution that triggered compilation took: " << compileexec << "s" << endl;
		cout << "Post-compile min, max (excl. compile execution), average (incl. compile execution), average (excl. compile execution): " << endl;
		cout << "\t" << postmin << "s, " << postmax << "s, " << postavgincl << "s, " << postavgexcl << "s" << endl;
		cout << "Total average with " << iterations << " iterations: " << avg << "s" << endl;
		if(memprof)
		{
			cout << "Max resident working set pre-JIT: " << prememoryusage << ((prememoryusage == prestart_wss) ? "kb (shadowed by startup)" : "kb") << endl;
			cout << "Max resident working set post-JIT: " << postmemoryusage << ((postmemoryusage == prestart_wss) ? "kb (shadowed by startup)" : "kb") << endl;
		}
		else
			cout << "(Memory profiling disabled due to error)" << endl;

		delete[] times;
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
