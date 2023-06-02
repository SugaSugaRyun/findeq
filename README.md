findeq
====
A program that finds all duplicate files under a specific path and its subpaths.

Install and Build
-----
To clone the repository you should have Git installed. Just run:

    $ git clone https://github.com/SugaSugaRyun/findeq.git

To build the library with some test program, run `make`. 

    $ make  

To compile directly, you'll need to add the following options

    $ -pthread

To delete *.o files and excutable file(test1~4), run
`make clean`.

    $ make clean

How to use
----
    ./findeq -t="NUM" -m="NUM" -o="FILE" DIR

-t : Create thread number of NUM (essential, maximum: 64)  
-m : Ignores all files whose size is less than NUM (default 1024)  
-o : Saves result to FILE (default stdout)  
DIR : target directory