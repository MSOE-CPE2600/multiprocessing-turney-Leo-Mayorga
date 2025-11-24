/*************************************
* Course: CPE 2600 112
* Filename: mandel.c
* Description: generates 50 zoom frames in parallel using multiple processes, writing each frame as a JPEG image for
			   for animation. Based on example code found here: https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html.
			   Converted to use jpg instead of BMP and other minor changes.
* Author: Leo Mayorga
* Date: 11/19/25
* Compile: make
* Run: ./mandel -p <number of processes> -t <number of threads>
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include "jpegrw.h"

#define NUM_FRAMES 50
#define ZOOM_FACTOR 0.9

// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );

// struct for thread data
typedef struct 
{
	imgRawImage *img;	// shared image buffer
	double xmin;		// left boundary
	double xmax;		// right boundary 
	double ymin;		// bottom boundary 
	double ymax;		// top boundary
	int max;			// max interations per pixel
	int y_start;		// starting row
	int y_end;			// ending row
} ThreadArgs;

static void compute_image( imgRawImage *img, double xmin, double xmax,
									double ymin, double ymax, int max, int num_threads);
static void show_help();
static double scale_for_frame(double start_scale, int frame);
static void *thread_func(void *args);

// calculate the zoom scale 
static double scale_for_frame(double start_scale, int frame)
{
	double result = start_scale;
	for (int i = 0; i < frame; i++)
	{
		result *= ZOOM_FACTOR;
	}
	return result;
}

int main( int argc, char *argv[] )
{
	char c;

	// These are the default configuration values used if no command line arguments are given
	double xcenter = 0.25;
	double ycenter = 0;
	double xscale = 4;
	int    image_width = 1000;
	int    image_height = 1000;
	int    max = 1000;

	// number of children processes 
	int num_procs = 0;
	// number of threads per image 
	int num_threads = 1;

	// For each command line argument given,
	// override the appropriate configuration value.

	while((c = getopt(argc,argv,"x:y:s:W:H:m:p:t:h"))!=-1) {
		switch(c) 
		{
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				xscale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'p':
				num_procs = atoi(optarg);
				break;
			case 't':
				num_threads = atoi(optarg);
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}

	if (num_procs <= 0)
	{
		fprintf(stderr, "Error: you must specify -p <num_processes>\n");
		return -1;
	}

	if (num_procs > NUM_FRAMES) {
		num_procs = NUM_FRAMES;
	}

	// 1-20 possible threads
	if (num_threads < 1)
	{
		num_threads = 1;
	}
	if (num_threads > 20)
	{
		num_threads = 20;
	}

	printf("mandel: center=(%lf,%lf) start_scale=%lf max=%d, frames=%d, procs=%d, threads=%d\n", xcenter, ycenter,
		xscale, max, NUM_FRAMES, num_procs, num_threads);

	int frame = 0;
	int running = 0;

	while (frame < NUM_FRAMES || running > 0)
	{
		// launch the new children
		while (running < num_procs && frame < NUM_FRAMES)
		{
			int my_frame = frame; // frame index
			double xscale_frame = scale_for_frame(xscale, my_frame);
			double yscale_frame = xscale_frame / image_width * image_height;

			pid_t pid = fork();
			if (pid< 0)
			{
				perror("fork");
				exit(1);
			}
			
			if (pid == 0) 
			{
				// child
				// filename for this frame
				char fname[64];
				snprintf(fname, sizeof(fname), "mandel%d.jpg", my_frame);
				// allocate a new raw image buffer for this frame
				imgRawImage* img = initRawImage(image_width, image_height);
				setImageCOLOR(img, 0);

				// Mandel image for each frame's zoom level and region
				// xmin, xmax, ymin, ymax
				compute_image(img, xcenter - xscale_frame/2, xcenter + xscale_frame/2, 
					ycenter - yscale_frame/2, ycenter + yscale_frame/2, max, num_threads);
				// store the generated image to a JPEG file
				storeJpegImageFile(img, fname);
				// free the image buffer 
				freeRawImage(img);
				_exit(0); // exit but don't return to main loop yet
			} else {
				// parent 
				// filename for this frame
				char fname[64];
				snprintf(fname, sizeof(fname), "mandel%d.jpg", my_frame);
				printf("Frame %d: %s (pid=%d)\n", my_frame, fname, pid); // print the process as it is created
				fflush(stdout); // make sure it prints right away
				running++; // increment children running
				frame++; // move on to the next frame
			}
		}

		// wait for the child to finish
		if (running > 0)
		{
			int status;
			pid_t done = wait(&status);
			if (done == -1)
			{
				perror("wait");
				exit(1);
			}
			(void)done;
			running--;
		}
	}
	return 0;
}


/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

static int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

// each thread renders a different horizontal are of rows
static void *thread_func(void *arg)
{
	ThreadArgs *t = (ThreadArgs *)arg;
	imgRawImage *img = t -> img;
	int width = img -> width;
	int height = img -> height;

	// loop over the thread's assigned rows
	for(int j = t->y_start; j < t -> y_end; j++)
	{
		// row j to a y coordinate in the complex plane
		double y = t->ymin + j * (t->ymax - t->ymin) / height;

		// loop over all of the columns in this row
		for (int i = 0; i < width; i++)
		{
			// column i to an x corrdinate
			double x = t->xmin + i * (t->xmax - t->xmin) / width;
			// calculate mandle iterations for this specific point
			int iters = iterations_at_point(x, y, t->max);
			// convert iteration count to a color and set the pixel 
			setPixelCOLOR(img, i, j, iteration_to_color(iters, t->max));
		}
	}
	return NULL;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/
static void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max, int num_threads)
{
	int width = img->width;
	int height = img->height;

	// make sure the entered thread count is valid
	if (num_threads < 1)
	{
		num_threads = 1;
	}
	if (num_threads > 20)
	{
		num_threads = 20;
	}
	if (num_threads > height)
	{
		// cannot have more threads than rows
		num_threads = height;
	}

	// single thread
	if (num_threads == 1)
	{
		for (int j = 0; j < height; j++)
		{
			double y = ymin + j * (ymax - ymin) / height;
			for (int i = 0; i < width; i++)
			{
				double x = xmin + i * (xmax - xmin) / width;
				int iters = iterations_at_point(x, y, max);
				setPixelCOLOR(img, i, j, iteration_to_color(iters, max));
			}
		}
		return;
	}

	// miltiple threads 
	pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
	ThreadArgs *args = malloc(sizeof(ThreadArgs) * num_threads);

	if (!threads || !args)
	{
		// if allocation fails
		if (threads) free(threads);
		if (args) free(args);

		for (int j = 0; j < height; j++)
		{
			double y = ymin + j * (ymax - ymin) / height;
			for (int i = 0; i < width; i++)
			{
				double x = xmin + i * (xmax - xmin) / width;
				int iters = iterations_at_point(x, y, max);
				setPixelCOLOR(img, i, j, iteration_to_color(iters, max));
			}
		}
		return;
	}

	// divide the image into rows as evenly as possible
	int rows_per_thread = height / num_threads;
	int extra = height %  num_threads;	// distribute the remaining rows per thread

	int current_row = 0;

	// create threads
	for (int t = 0; t < num_threads; t++)
	{
		int this_rows = rows_per_thread + (t < extra ? 1 : 0);

		args[t].img = img;
		args[t].xmin = xmin;
		args[t].xmax = xmax;
		args[t].ymin = ymin;
		args[t].ymax = ymax;
		args[t].max = max;
		args[t].y_start = current_row;				// starting row for this thread
		args[t].y_end = current_row + this_rows;	// one past the last row

		current_row = args[t].y_end;
		// lunch thread to render
		pthread_create(&threads[t], NULL, thread_func, &args[t]);
	}

	// wait for all of the threads to finish
	for (int t = 0; t < num_threads; t++)
	{
		pthread_join(threads[t], NULL);
	}

	free(threads);
	free(args);
}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
static int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    	The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  	X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  	Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  	Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> 	Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> 	Height of the image in pixels. (default=1000)\n");
	printf("-p <procs> 		Number of child processes to use\n");
	printf("-t <threads>	Number of threads per image (1-20)\n");
	printf("-o <file>   	Set output file. (default=mandel.bmp)\n");
	printf("-h          	Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}
