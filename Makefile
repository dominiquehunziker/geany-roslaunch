ROSDISTRO ?= kinetic
CC ?= gcc

CFLAGS += -fPIC $(shell pkg-config --cflags geany) -Wall -O3 -g
ROS_INCLUDE_DIR := -I/opt/ros/$(ROSDISTRO)/include
ROS_LIBRARY_DIR := -L/opt/ros/$(ROSDISTRO)/lib
ROS_LIBRARY := -lroslib

all: roslaunch.so

roswrapper.o: roswrapper.cpp roswrapper.h
	$(CC) -c -o $@ $< $(CFLAGS) $(ROS_INCLUDE_DIR)

plugin.o: plugin.c roswrapper.h
	$(CC) -c -o $@ $< $(CFLAGS)

roslaunch.so: roswrapper.o plugin.o
	$(CC) -o $@ -shared $^ $(CFLAGS) $(ROS_LIBRARY_DIR) $(ROS_LIBRARY)

clean:
	rm -f roslaunch.so *.o

install: roslaunch.so
	install -m 644 roslaunch.so $(shell pkg-config --variable=libdir geany)/geany

uninstall:
	rm -f $(shell pkg-config --variable=libdir geany)/geany/roslaunch.so

symlink: roslaunch.so
	ln -s ${CURDIR}/roslaunch.so $(shell pkg-config --variable=libdir geany)/geany
