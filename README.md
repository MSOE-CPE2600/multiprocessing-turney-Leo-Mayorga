# System Programming Lab 11 Multiprocessing

Author: Leo Mayorga 
Date: 11/23/24

### Overview
**Lab 11** generates 50 Mandlebrot images that zoom in progressively into a chosen coordinate. The images are produced in parallel, using multiple processes through fork() as desired by the user. 

**Lab 12** takes it further by implementing threads as well. Each process still renders a subset of the frames, but now each image is spilt across different threads that each computer different rows in parallel. 

Each frame is rendered as a JPEG file. All of the frames are then put together to create a video using ffmpeg.

### Runtime Table

| Processes | Threads | Runtime (sec) |
|-----------|---------|---------------|
| 1         | 1       | 337.631       |
| 1         | 2       | 178.533       |
| 1         | 4       | 102.304       |
| 2         | 1       | 167.135       |
| 2         | 2       | 94.688        |
| 2         | 4       | 62.203        |
| 3         | 1       | 117.133       |
| 3         | 2       | 69.7          |
| 3         | 4       | 44.54         |
| 4         | 1       | 96.201        |
| 4         | 2       | 55.046        |
| 4         | 4       | 50.041        |


### Runtime Results
![alt text](<Runtime vs. Threads for Various Process Counts-1.png>)

![alt text](Graph-1.png)

As the number of processes increased from 1 to 4, the total runtime decreased because more frames were rendered in parallel instead of being rendered sequentially. Performance improved from 1 process to around 10 processes because more frames were being rendered in parallel. However, after about 10-12 processes, the runtime stabilized and no longer improved by much.

For a fixed number of processes, increasing the number of threads per process, decreased the overall runtime because each image is being divided across more threads that compute different rows at the same time. As we increase the amount of threads, we notice that their benefits shrink until the CPU cores become saturated. 

**Which technique seemed to impact runtime more - multithreading or multiprocessing? Why?**
Multithreading had a slightly larger impact on runtime than multiprocessing. Across all number of processes, increasing the number of threads consistently brought the runtime down significantly. Increasing also helped, but the improvement was smaller compared to multiple threads. That makes sense because threads share the same memory space and have a lower overhead than processes.

**Was there a "sweet spot" where optimal (minimal) runtime was achieved?**
The fastest runtime was at around 4 processes and 4 threads with a time of 40.041 seconds. At that point, the program was fully utilizing the computer's CPU cores.

### Files
 1) jpegrw.h - header file for jpegrw.c to help read and write JPG files
 2) jpegrw.c - JPEG read/write helper
 3) mandel.c - renders each JPEG image

### Commands
-x <coord>    - x coordinate of zoom center  
-y <coord>    - y coordinate of zoom center  
-s <scale>    - starting scale (zoom out factor)  
-m <max>      - max number of iterations per pixel  
-W <pixel>    - width of the image in pixels (default == 1000)  
-H <pixel>    - height of the image in pixels (default == 1000)  
-p <num>      - number of processes to run in parallel  
-h            - display help menu  

### Compile
Command line: make

### Run
Default command line: ./mandel -p <num>  
Custom command line: ./ mandel -x 0.25 -y 0 -s 4 -m 1000 -W 1000 -H 1000 -p 12

### Creating the Animation
Command Line: ffmpeg -i mandel%d.jpg mandel.mp4