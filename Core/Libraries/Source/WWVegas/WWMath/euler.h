/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header: /Commando/Code/wwmath/euler.h 5     5/05/01 5:48p Jani_p $ */
/***********************************************************************************************
 ***                            Confidential - Westwood Studios                              ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Commando / G Math Library                                    *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/wwmath/euler.h                               $*
 *                                                                                             *
 *                       Author:: Greg_h                                                       *
 *                                                                                             *
 *                     $Modtime:: 5/04/01 8:37p                                               $*
 *                                                                                             *
 *                    $Revision:: 5                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "WWLib/always.h"
#include "matrix3d.h"
#include "quat.h"


/*********************************************************************

	Euler Order Types

	When creating an EulerAngles object, use one of the below
	constants to describe the axis convention.

	XYZ - order of the axes
	s/r - whether the rotations are applied to a static or
	      rotating frame.

*********************************************************************/


/* static axes */
extern const int	EulerOrderXYZs;
extern const int	EulerOrderXYXs;
extern const int	EulerOrderXZYs;
extern const int	EulerOrderXZXs;
extern const int	EulerOrderYZXs;
extern const int	EulerOrderYZYs;
extern const int	EulerOrderYXZs;
extern const int	EulerOrderYXYs;
extern const int	EulerOrderZXYs;
extern const int	EulerOrderZXZs;
extern const int	EulerOrderZYXs;
extern const int	EulerOrderZYZs;

/* rotating axes */
extern const int	EulerOrderXYZr;
extern const int	EulerOrderXYXr;
extern const int	EulerOrderXZYr;
extern const int	EulerOrderXZXr;
extern const int	EulerOrderYZXr;
extern const int	EulerOrderYZYr;
extern const int	EulerOrderYXZr;
extern const int	EulerOrderYXYr;
extern const int	EulerOrderZXYr;
extern const int	EulerOrderZXZr;
extern const int	EulerOrderZYXr;
extern const int	EulerOrderZYZr;


/*********************************************************************

	EulerAnglesClass

	The purpose for this class is mainly for conversion.  You can
	choose a convention for the order of your rotations and then
	convert matrices into a set of euler angles.  You don't really
	want to use this at run-time to convert matrices into angles.

	The guts of this implementation is based on the article in Graphics
	Gems IV by Ken Shoemake.  The original article is on page 222.

*********************************************************************/
class EulerAnglesClass
{

public:

	EulerAnglesClass() : Order(0) { Angle[0] = 0.0; Angle[1] = 0.0; Angle[2] = 0.0; };
	EulerAnglesClass(const Matrix3D & from,int order);
	void		From_Matrix(const Matrix3D & from,int order);
	void		To_Matrix(Matrix3D & M);

	double	Get_Angle(int i);

private:

	double	Angle[3];
	int		Order;

};
