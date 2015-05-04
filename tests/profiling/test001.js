// fibonacci numbers

function fib(n) {
	var current, last = 0, penult = 1;
	for(var i = 0; i < n; i++) {
		current = last + penult;
		penult = last;
		last = current;
	}
	return current;
}

function init() {
	NUM_ITERATIONS = 50;
	MAX_FIB = 1000;
}

function get_iterations() { return NUM_ITERATIONS; }

function setup() { }

function run() {
	print("Running fibonacci numbers - " + NUM_ITERATIONS + " iterations; computing fib(" + MAX_FIB + ") each time.");

	for(var i = 0; i < NUM_ITERATIONS; i++)
		fib(MAX_FIB);

	return "fib()";
}