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



#include <stdio.h>
#include <stdlib.h>
#include <structures.h>
#include <tkglobals.h>
#include <processno.h>
#include <modeno.h>
#include <filtcon.h>
#include <cdpmain.h>
#include <globcon.h>
#include <logic.h>

#include <pnames.h>
#include <sfsys.h>
//TW ADDED
#include <limits.h>

static int  pt_datareduce(double **q,double sharp,double flat,double *thisarray,int *bsize,dataptr dz);
static int  incremental_pt_datareduction(int array_no,int *bsize,double sharpness,dataptr dz);

	/* HEADWRITE */
static int  check_output_header_properties(dataptr dz);

	/* OTHER */
static int  datareduce(double **q,dataptr dz);
static int	sampbufs_init(dataptr dz);

/***************************** COMPLETE_OUTPUT *******************************/

int complete_output(dataptr dz)
{
	return(FINISHED);
}

/***************************** WRITE_EXACT_BYTES ******************************
 *
 * Writes a block of known length to file area of same length.
 * Checks it has all been written.
 */

int write_exact_bytes(char *buffer,int bytes_to_write,dataptr dz)
{
	int pseudo_bytes_to_write = bytes_to_write;
	int secs_to_write = bytes_to_write/SECSIZE;
	int bytes_written;
	if((secs_to_write * SECSIZE)!=bytes_to_write) {
		secs_to_write++;
		pseudo_bytes_to_write = secs_to_write * SECSIZE;
	}
	if((bytes_written = sfwrite(dz->ofd, buffer, pseudo_bytes_to_write))<0) {
		sprintf(errstr, "Can't write to output soundfile: (is hard-disk full?).\n");
		return(SYSTEM_ERROR);
	}
	if(bytes_written != bytes_to_write) {
		sprintf(errstr, "Incorrect number of bytes written\nbytes_written = %ld\n"
						"bytes_to_write = %ld\n (is hard-disk full?).\n",
						bytes_written,bytes_to_write);
		return(SYSTEM_ERROR);
	}
	dz->total_bytes_written += bytes_to_write;
	display_virtual_time(dz->total_bytes_written,dz);
	return(FINISHED);
}

/*************************** WRITE_BYTES ***********************/

int write_bytes(char *bbuf,int bytes_to_write,dataptr dz)
{
	int pseudo_bytes_to_write = bytes_to_write;
	int secs_to_write = bytes_to_write/SECSIZE;
	int bytes_written;
	if((secs_to_write * SECSIZE)!=bytes_to_write) {
		secs_to_write++;
		pseudo_bytes_to_write = secs_to_write * SECSIZE;
	}
	if((bytes_written = sfwrite(dz->ofd,bbuf,pseudo_bytes_to_write))<0) {
		sprintf(errstr,"Can't write to output soundfile: (is hard-disk full?).\n");
		return(SYSTEM_ERROR);
	}
	dz->total_bytes_written += bytes_to_write;
	dz->total_samps_written  = dz->total_bytes_written/sizeof(short);  /* IRRELEVANT TO SPEC */
	display_virtual_time(dz->total_bytes_written,dz);
	return(FINISHED);
}

/*********************** DISPLAY_VIRTUAL_TIME *********************/

void display_virtual_time(int bytes_sent,dataptr dz)
{
	int	mins;
	double	secs;
	int maxlen, minlen;
	unsigned int zargo = 0;
	int n, display_time;
	double float_time;
//TW UPDATE
	if(sloombatch)
		return;
	if(!sloom) {
		if(dz->process==HOUSE_COPY && dz->mode==COPYSF)
			secs = (double)bytes_sent/(double)(dz->infile->srate * dz->infile->channels * sizeof(short));
		else {
			switch(dz->outfiletype) {
			case(NO_OUTPUTFILE):
					return;
			case(SNDFILE_OUT):
				secs = (double)bytes_sent/(double)(dz->infile->srate * dz->infile->channels * sizeof(short));
				break;
			case(ANALFILE_OUT):
				secs = (double)(bytes_sent/dz->byteswanted) * dz->frametime;
				break;
			case(TEXTFILE_OUT):
				switch(dz->process) {
				case(INFO_PRNTSND):
		 			secs = (double)bytes_sent/(double)(dz->infile->srate * dz->infile->channels * sizeof(short));
					break;
				}	
				break;
	
			}
		}
		mins = (int)(secs/60.0);  	/* TRUNCATE */
		secs -= (double)(mins * 60);
		fprintf(stdout,"\r%ld min %5.2lf sec", mins, secs);
	} else {
		switch(dz->process_type) {
		case(EQUAL_ANALFILE):
			float_time   = min(1.0,(double)bytes_sent/(double)dz->infilesize[0]);
			break;
		case(MAX_ANALFILE):
			switch(dz->process) {
			default:
				maxlen = dz->infilesize[0];
				for(n=1;n<dz->infilecnt;n++)
					maxlen = max(maxlen,dz->infilesize[n]);
				float_time = (double)bytes_sent/(double)maxlen;
				break;
			}
			break;
		case(MIN_ANALFILE):
			minlen = dz->infilesize[0];
			for(n=1;n<dz->infilecnt;n++)
				minlen = min(minlen,dz->infilesize[n]);
			float_time = (double)bytes_sent/(double)minlen;
			break;
		case(BIG_ANALFILE):
	 		float_time = min(1.0,(double)bytes_sent/(double)dz->infilesize[0]);
			break;
	 	case(UNEQUAL_SNDFILE):
			switch(dz->process) {
			case(PVOC_SYNTH):
	 			float_time = min(1.0,(double)bytes_sent/(double)dz->tempsize);
				break;
			default:
	 			float_time = min(1.0,(double)bytes_sent/(double)dz->infilesize[0]);
				break;
			}
			break;
		case(EQUAL_SNDFILE):
 			float_time = min(1.0,(double)bytes_sent/(double)dz->infilesize[0]);
			break;
		case(TO_TEXTFILE):
			switch(dz->process) {
			case(INFO_PRNTSND):
				zargo = (dz->iparam[PRNT_END] - dz->iparam[PRNT_START])*sizeof(short);
	 			float_time = min(1.0,(double)bytes_sent/(double)zargo);
				break;
			}	
			break;
		}
		display_time = round(float_time * PBAR_LENGTH);
		fprintf(stdout,"TIME: %d\n",display_time);
		fflush(stdout);
	}
}

/***************************** WRITE_BYTES_TO_ELSEWHERE ******************************
 *
 * Allows bytes to be written to files other than standard outfile.
 * No byte-count checking.
 */

int write_bytes_to_elsewhere(int ofd, char *buffer,int bytes_to_write,dataptr dz)
{
	int pseudo_bytes_to_write = bytes_to_write;
	int secs_to_write = bytes_to_write/SECSIZE;
	int bytes_written;
	if((secs_to_write * SECSIZE)!=bytes_to_write) {
		secs_to_write++;
		pseudo_bytes_to_write = secs_to_write * SECSIZE;
	}
	if((bytes_written = sfwrite(ofd, (char *)buffer, pseudo_bytes_to_write))<0) {
		sprintf(errstr, "Can't write to output soundfile:  (is hard-disk full?).\n");
		return(SYSTEM_ERROR);
	}
	dz->total_bytes_written += bytes_to_write;
	return(FINISHED);
}

/****************************** WRITE_BRKFILE *************************/

int write_brkfile(FILE *fptext,int brklen,int array_no,dataptr dz)
{
	int n;
	double *p = dz->parray[array_no];
	for(n=0;n<brklen;n++) {
		fprintf(fptext,"%lf \t%lf\n",*p,*(p+1));
		p += 2;
	}
	return(FINISHED);
}

static int  datareduce(double **q,dataptr dz);

/***************** CONVERT_PCH_OR_TRANSPOS_DATA_TO_BRKPNTTABLE ***********************/

#define TONE_CENT	(.01) 	/* SEMITONES */

int convert_pch_or_transpos_data_to_brkpnttable(int *brksize,float *floatbuf,float frametime,int array_no,dataptr dz)
{
	int exit_status;
	double *q;
	float *p = floatbuf;
	float *endptab = floatbuf + dz->wlength;
	int n, bsize;
	double sharpness = TONE_CENT;	/* sharpness in semitones */

	if(dz->wlength<2) {
		sprintf(errstr,"Not enough pitchdata to convert to brktable.\n");
		return(DATA_ERROR);
	}
	q = dz->parray[array_no];
	n = 0;
	*q++ = 0.0;
	*q++ = (double)*p++;
	*q++ = frametime;
	*q++ = (double)*p++;
	n = 2;
	while(endptab-p > 0) {
		*q++ = (double)n * frametime;
		*q++ = (double)*p++;
		n++;
	}
	bsize = q - dz->parray[array_no];

	if((exit_status = incremental_pt_datareduction(array_no,&bsize,sharpness,dz))<0)
		return(exit_status);
	n = bsize;

	if((dz->parray[array_no] = (double *)realloc((char *)dz->parray[array_no],n*sizeof(double)))==NULL) {
		sprintf(errstr,"convert_pch_or_transpos_data_to_brkpnttable()\n");
		return(MEMORY_ERROR);
	}
	*brksize = n/2;
	return(FINISHED);
}
    
/***************************** PT_DATAREDUCE **********************
 *
 * Reduce data on passing from pitch or transposition to brkpnt representation.
 *
 * Note, these are identical, becuase the root-pitch from which pitch-vals are measured
 * cancels out in the maths (and doesn't of course occur in the transposition calc).
 *
 * Take last 3 points, and if middle point has (approx) same value as
 * a point derived by interpolating between first and last points, then
 * ommit midpoint from brkpnt representation.
 */

int pt_datareduce(double **q,double sharp,double flat,double *thisarray,int *bsize,dataptr dz)
{
	double interval;
	double *arrayend;
	double *p = (*q)-6, *midpair = (*q)-4, *endpair = (*q)-2;
	double startime = *p++;
	double startval = LOG2(*p++);								/* pitch = LOG(p/root) = LOG(p) - LOG(root) */
	double midtime  = *p++;
	double midval   = LOG2(*p++);								/* pitch = LOG(p/root) = LOG(p) - LOG(root) */
	double endtime  = *p++;
	double endval   = LOG2(*p);
	double valrange 	= endval-startval;	  					/* LOG(root) cancels out */
	double midtimeratio = (midtime-startime)/(endtime-startime);
	double guessval     = (valrange * midtimeratio) + startval;	 /*  -LOG(root) reintroduced */

	if((interval = (guessval - midval) * SEMITONES_PER_OCTAVE) < sharp && interval > flat) { /* but cancels again */
		arrayend = thisarray + *bsize;
		while(endpair < arrayend)
			*midpair++ = *endpair++;
		(*q) -= 2;
		*bsize -= 2;
	}
	return(FINISHED);
}

/***************************** INCREMENTAL_PT_DATAREDUCTION ******************************/

int incremental_pt_datareduction(int array_no,int *bsize,double sharp,dataptr dz)
{
	int exit_status;
	double flat;
	double *q;
	double sharp_semitones = LOG2(dz->is_sharp) * SEMITONES_PER_OCTAVE; 
	double *thisarray = dz->parray[array_no];
	while(*bsize >= 6 && sharp < sharp_semitones) { 
		flat = -sharp;
		q = thisarray + 4;
		while(q < thisarray + (*bsize) - 1) {
			q +=2;
			if((exit_status = pt_datareduce(&q,sharp,flat,thisarray,bsize,dz))<0)
				return(exit_status);
		}
		sharp *= 2.0;				/* interval-size doubles */
	}
	if(*bsize >= 6) { 
		sharp = sharp_semitones;
		flat = -sharp;
		q = thisarray + 4;
		while(q < thisarray + (*bsize) - 1) {
			q +=2;
			if((exit_status = pt_datareduce(&q,sharp,flat,thisarray,bsize,dz))<0)
				return(exit_status);
		}
	}
	return(FINISHED);
}

/****************************** SPLICE_MULTILINE_STRING ******************************/

void splice_multiline_string(char *str,char *prefix)
{
	char *p, *q, c;
	p = str;
	q = str;
	while(*q != ENDOFSTR) {
		while(*p != '\n' && *p != ENDOFSTR)
			p++;
		c = *p;
		*p = ENDOFSTR;
		fprintf(stdout,"%s %s",prefix,q);
		*p = c;
		if(*p == '\n')
			 p++;
		while(*p == '\n') {
			fprintf(stdout,"%s \n",prefix);
			p++;
		}
		q = p;
		p++;
	}
}
