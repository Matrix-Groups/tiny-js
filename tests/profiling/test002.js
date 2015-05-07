// factorial

function factorial(n) {
	var total = 1;
	for(var i = 1; i <= n; i++) {
		total = total * i;
	}
	return total;
}

function init() {
	NUM_ITERATIONS = 50;
	MAX_FACT = 1000;
}

function get_iterations() { return NUM_ITERATIONS; }

function setup() { }

function run() {
	print("Running factorial - " + NUM_ITERATIONS + " iterations; computing factorial(" + MAX_FACT + ") each time.");

	for(var i = 0; i < NUM_ITERATIONS; i++)
		fib(MAX_FACT);

	return "factorial()";
}