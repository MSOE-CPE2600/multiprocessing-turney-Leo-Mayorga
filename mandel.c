/*************************************
* Course: CPE 2600 112
* Filename: mandel.c
* Description: generates 50 zoom frames in parallel using multiple processes, writing each frame as a JPEG image for
			   for animation. Based on example code found here: https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html.
			   Converted to use jpg instead of BMP and other minor changes.
* Author: Leo Mayorga
* Date: 11/19/25
* Compile: make
* Run: ./mandel -p <number of processes>
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "jpegrw.h"

#define NUM_FRAMES 50
#define ZOOM_FACTOR 0.9

// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void compute_image( imgRawImage *img, double xmin, double xmax,
									double ymin, double ymax, int max );
static void show_help();
static double scale_for_frame(double start_scale, int frame);

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

	// For each command line argument given,
	// override the appropriate configuration value.

	while((c = getopt(argc,argv,"x:y:s:W:H:m:p:h"))!=-1) {
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

	printf("mandel: center=(%lf,%lf) start_scale=%lf max=%d, frames=%d, procs=%d\n", xcenter, ycenter,
		xscale, max, NUM_FRAMES, num_procs);

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
					ycenter - yscale_frame/2, ycenter + yscale_frame/2, max);
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

int iterations_at_point( double x, double y, int max )
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

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max )
{
	int i,j;

	int width = img->width;
	int height = img->height;

	// For every pixel in the image...

	for(j=0;j<height;j++) {

		for(i=0;i<width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = xmin + i*(xmax-xmin)/width;
			double y = ymin + j*(ymax-ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(img,i,j,iteration_to_color(iters,max));
		}
	}
}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}
