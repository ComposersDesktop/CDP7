/*
 * Copyright (c) 1983-2013 Trevor Wishart and Composers Desktop Project Ltd
 * http://www.trevorwishart.co.uk
 * http://www.composersdesktop.com
 *
 This file is part of the CDP System.

    The CDP System is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    The CDP System is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with the CDP System; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
 *
 */



#define IS_SPEC_COMPILE		(1)

int  specaccu(dataptr dz);
int  specexag(int *zero_set,dataptr dz);
int  specfold(dataptr dz);
int  specfocus(int *peakscore,int *descnt,int *least,dataptr dz);
int  outer_focu_loop(dataptr dz);
int  specfreeze(dataptr dz);
int  specfreeze2(dataptr dz);
int  specstep(dataptr dz);


