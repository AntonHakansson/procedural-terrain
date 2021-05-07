# Quick Introduction To Doing The Graphics Labs in Linux

## Note
This is only recommended for persons familiar and comfortable
with a Linux environment. Only limited support is provided, as not all
lab assistants may be able to answer your questions if you run into
problems.

The labs will *NOT* build on StuDAT computers since some of the external
libraries aren't available. This guide is meant for those of you that want
to run the labs on your own machines.

## Description
The labs are built with the build software CMake, which you will need to install.

You will also need to install the necessary third-party libraries needed for the labs. 
The libraries that you need are SDL2, GLEW and glm. You will also need to have an implementation of OpenGL 
installed on your system e.g. Mesa. 

Install CMake and these libraries with your package manager of choice:
```shell
apt-get install ...
yum install ...
pacman -S ...
...
```

If you are missing any necessary libraries CMake will complain when you are trying to build.

Now to building the labs. Change directory to the root directory containing the labs.

Make a directory called 'build' and enter it:
``` shell
mkdir build
cd build
```

Prepare the build files by typing:
``` shell
cmake ..
```
or
``` shell
cmake DCMAKE_BUILD_TYPE=Release ..
```

for a debug vs release build.

Now you can build the labs by simply typing:
``` shell
make
```

in the same directory.

The executable for each lab is now located in the corresponding directory in the build folder e.g. lab2-textures/lab2. 
