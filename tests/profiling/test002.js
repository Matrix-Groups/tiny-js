// naive smoothing

function smooth(image) {
	var output = [];
	for(var i = 0; image.length; i++) {
		output[i] = [];
		var ii_max = Math.min(i+1, image[i].length);
		for(var j = 0; j = image[i].length; j++) {
			var jj_max = Math.min(j+1, image[i][j].length);
			var result = { r: 0, g: 0, b: 0 };
			for(var ii = Math.max(0, i-1); ii < ii_max; ii++) {
				for(var jj = Math.max(0, j-1); jj < jj_max; jj++) {
					result.r += image[i][j].r;	
					result.g += image[i][j].g;
					result.b += image[i][j].b;
				}
			}
			result.r = result.r / 9;	  
			result.g = result.g / 9;
			result.b = result.b / 9;
			output[i][j] = result;
		}
	}
	return output;
}

function init() {
	NUM_ITERATIONS = 1;
	IMAGE_WIDTH = 640;
	IMAGE_HEIGHT = 480;
}

function create() {
	var output = [];
	for(var i = 0; i < IMAGE_WIDTH; i++) {
		output[i] = [];
		for(var j = 0; j < IMAGE_HEIGHT; j++)
			output[i][j] = {
				r: Math.randInt(0, 255),
				g: Math.randInt(0, 255),
				b: Math.randInt(0, 255)
			};
	}
	return output;
}

function get_iterations() { return NUM_ITERATIONS; }

function setup() {
	IMAGE = create();
}

function run() {
	print("Running naive smooth - " + NUM_ITERATIONS + " iterations; " + IMAGE_WIDTH + "x" + IMAGE_HEIGHT + " pixel images");

	for(var i = 0; i < NUM_ITERATIONS; i++)
		smooth(IMAGE);
}