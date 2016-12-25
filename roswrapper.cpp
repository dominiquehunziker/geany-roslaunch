/*
 * roswrapper.cpp
 * 
 * Copyright 2016 Dominique Hunziker <dominique.hunziker@gmail.com>
 * 
 * This file is part of geany-roslaunch.
 *
 * geany-roslaunch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * geany-roslaunch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with geany-roslaunch. If not, see <http://www.gnu.org/licenses/>.
 */


#include "roswrapper.h"

#include <cstdlib>
#include <cstring>

#include <ros/package.h>

char* get_package_path(const char* pkg) {
    const std::string path = ros::package::getPath(pkg);

    if (path.empty())
        return NULL;
    
    char* c_path = (char*) std::malloc(path.length() + 1);
    std::strcpy(c_path, path.c_str());
    
    return c_path;
}

