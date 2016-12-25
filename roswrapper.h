/*
 * roswrapper.h
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

#if defined(__cplusplus)
extern "C" {
#endif

char* get_package_path(const char* pkg);

#if defined(__cplusplus)
}
#endif
