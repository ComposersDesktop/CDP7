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



//	MODIFICATIONS TO BUFFERS OR CODE FOR MULTICHANNEL WORK

// LIB

/**********************************	tklib3.c *************************************/

/*************************** CREATE_SNDBUFS **************************/

/* 2009 MULTICHANNEL */

int create_sndbufs(dataptr dz)
{
	int n;
	size_t bigbufsize;
	int framesize;
	framesize = F_SECSIZE * dz->infile->channels;
	if(dz->sbufptr == 0 || dz->sampbuf==0) {
		sprintf(errstr,"buffer pointers not allocated: create_sndbufs()\n");
		return(PROGRAM_ERROR);
	}
	bigbufsize =  (size_t)Malloc(-1);
	bigbufsize /= dz->bufcnt;
	if(bigbufsize <=0)
		bigbufsize  = framesize * sizeof(float);

	dz->buflen = (int)(bigbufsize / sizeof(float));	
	dz->buflen = (dz->buflen / framesize)  * framesize;
	bigbufsize = dz->buflen * sizeof(float);
	if((dz->bigbuf = (float *)malloc(bigbufsize  * dz->bufcnt)) == NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY to create sound buffers.\n");
		return(PROGRAM_ERROR);
	}
	for(n=0;n<dz->bufcnt;n++)
		dz->sbufptr[n] = dz->sampbuf[n] = dz->bigbuf + (dz->buflen * n);
	dz->sampbuf[n] = dz->bigbuf + (dz->buflen * n);
	return(FINISHED);
}

// EDITSF

/**********************************	ap_edit.c *************************************/

/**************************** RANDCUTS_PCONSISTENCY ****************************/

int randcuts_pconsistency(dataptr dz)
{
	int chans = dz->infile->channels;
	int excess, endunit_len;
	double duration = (double)(dz->insams[0]/chans)/(double)dz->infile->srate;
    initrand48();
	dz->iparam[RC_CHCNT] = round(duration/dz->param[RC_CHLEN]);
	if(dz->param[RC_SCAT] > (double)dz->iparam[RC_CHCNT]) {
		sprintf(errstr,"Scatter value cannot be greater than infileduration/chunklength.\n");
		return(DATA_ERROR);
	}    
	dz->iparam[RC_UNITLEN] = (int)round(dz->insams[0]/dz->iparam[RC_CHCNT]);
/* 2009 MULTICHANNEL --> */
	excess = dz->iparam[RC_UNITLEN] % chans;
	dz->iparam[RC_UNITLEN] -= excess;
/* <-- 2009 MULTICHANNEL */
/* OLD
	if(ODD(dz->iparam[RC_UNITLEN]))   
		dz->iparam[RC_UNITLEN]--;
*/
	excess = dz->insams[0] - (dz->iparam[RC_UNITLEN] * dz->iparam[RC_CHCNT]);
	endunit_len = dz->iparam[RC_UNITLEN] + excess;
	if((dz->lparray[0] = (int *)malloc(dz->iparam[RC_CHCNT] * sizeof(int))) ==NULL) {
		sprintf(errstr,"Insufficient space to store block sizes\n");
		return(MEMORY_ERROR);
	}
	if(dz->param[RC_SCAT] > 1.0) {
		dz->iparam[RC_SCAT] = round(dz->param[RC_SCAT]);
		dz->iparam[RC_SCATGRPCNT] = (int)(dz->iparam[RC_CHCNT]/dz->iparam[RC_SCAT]);
		dz->iparam[RC_ENDSCAT]    = (int)(dz->iparam[RC_CHCNT] - (dz->iparam[RC_SCATGRPCNT] * dz->iparam[RC_SCAT]));
		dz->iparam[RC_RANGE]      = dz->iparam[RC_UNITLEN] * dz->iparam[RC_SCAT];
		dz->iparam[RC_ENDRANGE]   = ((dz->iparam[RC_ENDSCAT]-1) * dz->iparam[RC_UNITLEN]) + endunit_len;
	} else if(dz->param[RC_SCAT] > 0.0)
		dz->iparam[RC_SCAT] = 0;		
	else
		dz->iparam[RC_SCAT] = -1;		
	return(FINISHED);
}

/**********************************	twixt.c *************************************/

/*************************** GENERATE_SPLICELEN **************************/

void generate_splicelen(dataptr dz)
{
	double splicetime;
	int j;

	dz->iparam[IS_SHSECSIZE] = /*SECSIZE/sizeof(short)*/ F_SECSIZE;
	splicetime = dz->param[IS_SPLEN] * MS_TO_SECS;
	j = round(splicetime * dz->infile->srate);
	j *= dz->infile->channels;
/* 2009 MULTICHANNEL --> */
//	j %= dz->iparam[IS_SHSECSIZE];
//	j = max(j,dz->iparam[IS_SHSECSIZE]);
/* <-- 2009 MULTICHANNEL */
	SPLICELEN = j;
	dz->param[IS_SPLICETIME] = (double)(SPLICELEN/dz->infile->channels)/(double)dz->infile->srate;
}

// HOUSKEEP

/**********************************	channels.c *************************************/

/*********************************** DO_CHANNELS ****************************/
/* RWD NB changes to ensure outfile is created with correct chan count. */
/* need full set of oufile props in dz, so we NEVER use dz-infile to spec the outfile! */
/* TODO: support n_channels, e.g. for zero, channel etc */

int do_channels(dataptr dz)
{
	int exit_status;
	int chans = dz->infile->channels, start_chan = 0, end_chan = 0;
//TW REVISION Dec 2002
//	char *outfilename, *outfilenumber;
	char *outfilename;
	char prefix[] = "_c";
	int namelen;
	int prefixlen = strlen(prefix);
	int infilesize, outfilesize;
	int k, m, n, j;
	float *buf = dz->bigbuf;
	int not_zeroed = 0, zeroed = 0, totlen;
	char *p, *q, *r;

	switch(dz->mode) {
	case(HOUSE_CHANNEL):
		dz->iparam[CHAN_NO]--;	/* change to internal numbering */
		break;
	case(HOUSE_ZCHANNEL):
		dz->iparam[CHAN_NO]--;	/* change to internal numbering */
		zeroed     =  dz->iparam[CHAN_NO];
		not_zeroed = !dz->iparam[CHAN_NO];
		break;
	}
	switch(dz->mode) {
	case(HOUSE_CHANNEL):
	case(HOUSE_CHANNELS):
		dz->buflen *= chans;				/* read chan * (contiguous) buffers */
		if(!sloom) {
			namelen = strlen(dz->wordstor[0]);
			q = dz->wordstor[0];
			r = dz->wordstor[0] + namelen;
			p = r - 1;
			while((*p != '\\') && (*p != '/') && (*p != ':')) {
				p--	;
				if(p < dz->wordstor[0])
					break;
			}
			if(p > dz->wordstor[0]) {
				p++;
				while(p <= r)
					*q++ = *p++;
			}
		}
		namelen   = strlen(dz->wordstor[0]);
		if(!sloom)
			totlen = namelen + prefixlen + 2;
		else
			totlen = namelen + 2;
		if((outfilename = (char *)malloc(totlen * sizeof(char)))==NULL) {
			sprintf(errstr,"INSUFFICIENT MEMORY for outfile name.\n");
			return(MEMORY_ERROR);
		}
		switch(dz->mode) {
		case(HOUSE_CHANNEL):
			dz->tempsize = dz->insams[0]/chans;	/* total outfile size */
			start_chan = dz->iparam[CHAN_NO];
			end_chan   = dz->iparam[CHAN_NO]+1;
			break;
		case(HOUSE_CHANNELS):
			dz->tempsize = dz->insams[0]/dz->infile->channels;
			start_chan = 0;
			end_chan   = dz->infile->channels;
			break;
		}	
		dz->process_type = EQUAL_SNDFILE;	/* allow sndfile(s) to be created */
//#ifdef NOTDEF
		/*RWD: GRRR! */
		dz->infile->channels = MONO;		/* force outfile(s) to be mono    */
//#endif
		infilesize  = dz->insams[0];
		outfilesize = dz->insams[0]/chans;
		for(k=start_chan,j=0;k<end_chan;k++,j++) {
			if(sndseekEx(dz->ifd[0],0,0)<0) {
				sprintf(errstr,"sndseek failed.\n");
				return(SYSTEM_ERROR);
			}
			dz->total_samps_written = 0;
			dz->samps_left = dz->insams[0];
			dz->total_samps_read = 0;	/* NB total_samps_read NOT reset */

			strcpy(outfilename,dz->wordstor[0]);
			if(!sloom) {
//TW REVISION Dec 2002
				insert_new_chars_at_filename_end(outfilename,prefix);
				insert_new_number_at_filename_end(outfilename,k+1,0);
			} else
				insert_new_number_at_filename_end(outfilename,j,1);
			dz->insams[0] = outfilesize;   /* creates smaller outfile */
			if((exit_status = create_sized_outfile(outfilename,dz))<0) {
				sprintf(errstr,"Cannot open output file %s\n", outfilename);
				dz->insams[0] = infilesize;
				free(outfilename);
				return(DATA_ERROR);
			}
			dz->insams[0] = infilesize;		/* restore true value */
			while(dz->samps_left) {
				if((exit_status = read_samps(buf,dz))<0) {
					free(outfilename);
					return(exit_status);
				}
				for(n=k,m=0;n<dz->ssampsread;n+=chans,m++)
					buf[m] = buf[n];
				if(dz->ssampsread > 0) {
					if((exit_status = write_exact_samps(buf,dz->ssampsread/chans,dz))<0) {
						free(outfilename);
						return(exit_status);
					}
				}
			}
			dz->outfiletype  = SNDFILE_OUT;			/* allows header to be written  */
			/* RWD: will eliminate this eventually */
			if((exit_status = headwrite(dz->ofd,dz))<0) {
				free(outfilename);
				return(exit_status);
			}
			if(k < end_chan - 1) {
				if((exit_status = reset_peak_finder(dz))<0)
					return(exit_status);
				if(sndcloseEx(dz->ofd) < 0) {
					fprintf(stdout,"WARNING: Can't close output soundfile %s\n",outfilename);
					fflush(stdout);
				}
				/*free(outfilename);*/ /*RWD 25:9:2001 used again! */
				dz->ofd = -1;
			}
		}
		break;
	case(HOUSE_ZCHANNEL):
	case(STOM):
	case(MTOS):
		switch(chans) {
		case(MONO): 
			switch(dz->mode) {
			case(HOUSE_ZCHANNEL):
				/*RWD*/
				dz->outfile->channels = 2;

				dz->tempsize = dz->insams[0] * 2;	break;	/* total output size */
			case(STOM):
				sprintf(errstr,"This file is already mono!!\n");	return(GOAL_FAILED);
			case(MTOS):
				/*RWD*/
				dz->outfile->channels = 2;

				dz->tempsize = dz->insams[0] * 2;	break;	/* total output size */
			
			}
			break;
		case(STEREO): 
			dz->buflen *= chans;	/* read chans * (contiguous) buffers */
			switch(dz->mode) {
			case(HOUSE_ZCHANNEL):				
				/*RWD*/
				dz->outfile->channels = 2;

				dz->tempsize = dz->insams[0];		break;	/* total output size */
			case(STOM):
				/*RWD*/
				dz->outfile->channels = 1;
				
				dz->tempsize = dz->insams[0]/2;		break;	/* total output size */
			case(MTOS): sprintf(errstr,"This file is already stereo!!\n");	return(GOAL_FAILED);
			}
			break;
/* MULTICHANNEL 2009 --> */
		default:
			switch(dz->mode) {
			case(STOM):
			case(MTOS):
				sprintf(errstr,"This process does not work with multichannel files!!\n");	return(GOAL_FAILED);
				break;
			case(HOUSE_ZCHANNEL):
				dz->outfile->channels = dz->infile->channels;
				break;
			}
			break;
/* <-- MULTICHANNEL 2009 */
		}
		/*RWD NOW, we can create the outfile! */
		/*RWD April 2005 need this for wave-ex */
		if(dz->outfile->channels > 2 || dz->infile->stype > SAMP_FLOAT){
			SFPROPS props, inprops;
			dz2props(dz,&props);			
			props.chans = dz->outfile->channels;
			props.srate = dz->infile->srate;
			/* RWD: always need to hack it one way or another!*/
			props.samptype = dz->infile->stype;
			if(dz->ifd && dz->ifd[0] >=0) {
				if(snd_headread(dz->ifd[0], &inprops)){
					/* if we receive an old WAVE 24bit file, want to write new wave-ex one! */
					if(inprops.chformat == STDWAVE)
						props.chformat = MC_STD;
					else
						props.chformat = inprops.chformat;
				}
			}
#ifdef _DEBUG
			printf("DEBUG: writing WAVE_EX outfile\n");
#endif
			dz->ofd = sndcreat_ex(dz->outfilename,-1,&props,SFILE_CDP,CDP_CREATE_NORMAL); 
			if(dz->ofd < 0){
				sprintf(errstr,"Cannot open output file %s\n", dz->outfilename);
				return(DATA_ERROR);
			}
		}
		else
		//create outfile here, now we have required format info	
			dz->ofd = sndcreat_formatted(dz->outfilename,-1,dz->infile->stype,dz->outfile->channels,
									dz->infile->srate,									
									CDP_CREATE_NORMAL);
//TW ADDED, for peak chunk
		dz->outchans = dz->outfile->channels;
		establish_peak_status(dz);
//TW: CHANGED
//		if(dz->ofd < 1)
		if(dz->ofd < 0)
			return DATA_ERROR;

		while(dz->samps_left) {
			if((exit_status = read_samps(buf,dz))<0) {
				return(exit_status);
			}
			if(dz->ssampsread > 0) {
				switch(chans) {
				case(MONO): 
					switch(dz->mode) {
					case(HOUSE_ZCHANNEL):
						for(n=dz->ssampsread-MONO,m=(dz->ssampsread*STEREO)-STEREO;n>=0;n--,m-=2) {
							buf[m+zeroed]     = (float)0;
							buf[m+not_zeroed] = buf[n];														   
						}
						if((exit_status = write_samps(buf,dz->ssampsread * STEREO,dz))<0)
							return(exit_status);
						break;
					case(MTOS):
						for(n=dz->ssampsread-MONO,m=(dz->ssampsread*STEREO)-STEREO,k=m+1;n>=0;n--,m-=2,k-=2) {
							buf[m] = buf[n];
							buf[k] = buf[n];
						}
						if((exit_status = write_samps(buf,dz->ssampsread * STEREO,dz))<0)
							return(exit_status);
						break;
					}		
					break;
				case(STEREO): 
					switch(dz->mode) {
					case(HOUSE_ZCHANNEL):
						for(n=dz->iparam[CHAN_NO];n< dz->ssampsread;n+=2)
							buf[n] = (float)0;
						if((exit_status = write_samps(buf,dz->ssampsread,dz))<0) {
							return(exit_status);
						}
						break;
					case(STOM):
						if(dz->vflag[CHAN_INVERT_PHASE]) {
							for(n=0,m=0;n< dz->ssampsread;n+=2,m++)
								buf[m] = (float)((buf[n] - buf[n+1])/2.0);
						} else {
							for(n=0,m=0;n< dz->ssampsread;n+=2,m++)
								buf[m] = (float)((buf[n] + buf[n+1])/2.0);
						}
						if((exit_status = write_samps(buf,dz->ssampsread/2,dz))<0) {
							return(exit_status);
						}
						break;
					}
					break;
				default:		// HOUSE_ZCHANNEL
					for(n=dz->iparam[CHAN_NO];n< dz->ssampsread;n+=dz->infile->channels)
						buf[n] = (float)0;
					if((exit_status = write_samps(buf,dz->ssampsread,dz))<0) {
						return(exit_status);
					}
					break;

				}
			}
		}
		switch(dz->mode) {
		case(HOUSE_ZCHANNEL):	dz->infile->channels = dz->outfile->channels;	break;
		case(STOM):				dz->infile->channels = MONO;  	break;
		case(MTOS):			 	dz->infile->channels = STEREO;	break;
		}
		break;
	}
	return(FINISHED);
}

/************************************** CODE WHERE i'M NOT SURE UPDATES HAVE BEEN MADE ************************/

/***************************** clean.c ***********************************/

/******************************* CREATE_CUTGATE_BUFFER ****************************/
//ARGGGGH! Parameters DEFINED as BYTES!!!!
//TW sticking to old buffer-sector protocol: enveloping algo depends on this ......
int create_cutgate_buffer(dataptr dz)
{   
	int k, envelope_space,total_bufsize;
	int numsecs;
	int samp_blocksize = dz->iparam[CUTGATE_SAMPBLOK] * sizeof(float);
	size_t bigbufsize;

	bigbufsize = (size_t)Malloc(-1);
	if((bigbufsize = (bigbufsize/samp_blocksize) * samp_blocksize)<=0)
		bigbufsize = samp_blocksize;
	dz->buflen = (int)(bigbufsize/sizeof(float));
	/* dz->buflen needed for searchpars() */
    searchpars(dz);
	if((k = 
	dz->iparam[CUTGATE_NUMSECS]/dz->iparam[CUTGATE_SAMPBLOK])* dz->iparam[CUTGATE_SAMPBLOK] < dz->iparam[CUTGATE_NUMSECS])
		k++;
	numsecs = k * dz->iparam[CUTGATE_SAMPBLOK];
	envelope_space = (numsecs + dz->iparam[CUTGATE_WINDOWS]) * sizeof(float);
 	total_bufsize = bigbufsize + envelope_space + (F_SECSIZE * sizeof(float));	/* overflow sector in sndbuf */
	if((dz->bigbuf = (float *)malloc((size_t)total_bufsize)) == NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY for sound and envelope.\n");
		return(MEMORY_ERROR);
	}
	dz->sampbuf[ENV] = dz->bigbuf + dz->buflen + F_SECSIZE;
	return(FINISHED);
}

/*************************************** ap_house ***********************************/

/*************************** CREATE_SNDBUFS **************************/

int create_bakup_sndbufs(dataptr dz)
{
	int n;
	int insert_samps, insert_secs;
	size_t bigbufsize;
    int framesize = F_SECSIZE * dz->infile->channels;
	
	insert_samps = dz->infile->srate * dz->infile->channels;	/* 1 second in samps */
	insert_samps = (int)round(BAKUP_GAP * insert_samps);
	if((insert_secs = insert_samps/framesize) * framesize != insert_samps)
		insert_secs++;
	insert_samps = insert_secs * framesize;
	if(dz->sbufptr == 0 || dz->sampbuf == 0) {
		sprintf(errstr,"buffer pointers not allocated: create_sndbufs()\n");
		return(PROGRAM_ERROR);
	}
	bigbufsize = (size_t)Malloc(-1);
	bigbufsize /= dz->bufcnt;
	dz->buflen = (int)(bigbufsize/sizeof(float));
	if((dz->buflen = (dz->buflen/framesize) * framesize)<=insert_samps) {
		dz->buflen  = insert_samps;
	}
	if((dz->bigbuf = (float *)malloc((size_t)(dz->buflen * sizeof(float) * dz->bufcnt))) == NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY to create sound buffers.\n");
		return(PROGRAM_ERROR);
	}
	for(n=0;n<dz->bufcnt;n++)
		dz->sbufptr[n] = dz->sampbuf[n] = dz->bigbuf + (dz->buflen * n);
	dz->sampbuf[n] = dz->bigbuf + (dz->buflen * n);
	return(FINISHED);
}

/************************************** ap_grain *********************************/

/*************************** CREATE_GRAIN_SNDBUFS **************************/

int create_grain_sndbufs(dataptr dz)
{
	size_t bigbufsize;
	int zz, n;
//TW
//	int modulus = FSECSIZE;
	int modulus = FSECSIZE * dz->infile->channels;
	if(dz->sbufptr == 0 || dz->sampbuf==0) {
		sprintf(errstr,"buffer pointers not allocated: create_grain_sndbufs()\n");
		return(PROGRAM_ERROR);
	}
	bigbufsize = (size_t) Malloc(-1);

	dz->buflen = (int)(bigbufsize / sizeof(float));

	dz->buflen /= dz->bufcnt;
	if((zz = (int)round(dz->param[GR_BLEN] * dz->infile->srate)) > dz->buflen)
		dz->buflen = zz;
	dz->iparam[/*GR_WSIZE_BYTES*/GR_WSIZE_SAMPS] = 0;
	if(dz->vflag[GR_ENVTRACK] && !flteq(dz->param[GR_WINSIZE],0.0)) {
		/* may be able to eliminate this ...?? */
		establish_grain_envelope_windowsize_in_samps(dz);	
		modulus = dz->iparam[/*GR_WSIZE_BYTES*/GR_WSIZE_SAMPS];
	}
//TW most importantly, modulo-channels calc now moved AFTER "dz->buflen /= dz->bufcnt"
// so divided-buffers still multiples of chans
	if((dz->buflen  = (dz->buflen/modulus) * modulus)<=0) {
		dz->buflen  = modulus;
	}
	/*dz->buflen = dz->bigbufsize/sizeof(short);*/
	if((dz->bigbuf = (float *)malloc((size_t)(dz->buflen * dz->bufcnt)* sizeof(float))) == NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY to create sound buffers.\n");
		return(MEMORY_ERROR);
	}
	for(n=0;n<dz->bufcnt;n++)
		dz->sbufptr[n] = dz->sampbuf[n] = dz->bigbuf + (dz->buflen * n);
	dz->sampbuf[n] = dz->bigbuf + (dz->buflen * n);
	return(FINISHED);
}

/******************************* radical.c ****************************/

/******************************* CREATE_REVERSING_BUFFERS *******************************/

int create_reversing_buffers(dataptr dz)
{
//TW BIG SIMPLIFICATION as buffer sector-alignment abolished
	size_t bigbufsize;
	bigbufsize = (size_t)Malloc(-1);
	dz->buflen = (int)(bigbufsize / sizeof(float));
	dz->buflen = (dz->buflen / dz->infile->channels) * dz->infile->channels;

	dz->iparam[REV_BUFCNT] = dz->insams[0]/dz->buflen;
	if(dz->iparam[REV_BUFCNT] > 0)
		dz->iparam[REV_RSAMPS] 	  = dz->insams[0] - (dz->iparam[REV_BUFCNT] * dz->buflen);
	if((dz->bigbuf = (float *)malloc(dz->buflen * sizeof(float)))==NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY for sound buffers.\n");
		return(MEMORY_ERROR);
	}
	dz->sampbuf[0] = dz->bigbuf; 
	return FINISHED;
}

/***************************CREATE_SHRED_BUFFERS **************************/
//TW replaced by global
//#define SHRED_SECSIZE (256)
/* RWD sigh..... */
int create_shred_buffers(dataptr dz)
{
	int exit_status;
	int bigfilesize, file_size_in_frames;
	double bufs_per_file, lchunkcnt; 
	size_t basic_bufchunk, bufchunk =  (size_t) Malloc(-1);
	int framesize = F_SECSIZE * dz->infile->channels;

	bufchunk /= sizeof(float);
	basic_bufchunk = bufchunk;
	for(;;) {
		if((dz->buflen = (int) bufchunk) < 0) {
			sprintf(errstr,"INSUFFICIENT MEMORY to allocate sound buffer.\n");
			return(MEMORY_ERROR);
		}
		dz->buflen  = (dz->buflen/(framesize * dz->bufcnt)) * framesize * dz->bufcnt;
		dz->buflen /= dz->bufcnt;
	/* NEW --> */
		if(dz->buflen <= 0)
			dz->buflen  = framesize;
	/* <-- NEW */
		file_size_in_frames = dz->insams[0]/framesize;
		if(framesize * file_size_in_frames < dz->insams[0])
			file_size_in_frames++;
		bigfilesize = file_size_in_frames * framesize;
		if(bigfilesize <= dz->buflen)
			dz->buflen  = bigfilesize;
		if((dz->bigbuf = (float *)malloc((dz->buflen * dz->bufcnt) * sizeof(float)))==NULL) {
			sprintf(errstr,"INSUFFICIENT MEMORY to allocate sound buffer.\n");
			return(MEMORY_ERROR);
		}
		/*dz->buflen = dz->bigbufsize/sizeof(short);*/
		if((exit_status = check_for_too_small_buf(dz))>=0)
			break;
		bufchunk += basic_bufchunk;
	}
	dz->sampbuf[0] = dz->bigbuf;
	dz->sampbuf[1] = dz->sampbuf[0] + dz->buflen;
	if(bigfilesize == dz->buflen) {
		dz->iparam[SHR_LAST_BUFLEN] = dz->insams[0];	/* i.e. buflen = true filelen */
		dz->iparam[SHR_LAST_CHCNT]  = dz->iparam[SHR_CHCNT];
		dz->iparam[SHR_LAST_SCAT]   = dz->iparam[SHRED_SCAT];
	} else {
		bufs_per_file = (double)dz->insams[0]/(double)dz->buflen;
		lchunkcnt     = (double)dz->iparam[SHR_CHCNT]/bufs_per_file;
		dz->iparam[SHR_CHCNT]       = round(lchunkcnt);
		dz->iparam[SHRED_SCAT]      = min(dz->iparam[SHRED_SCAT],dz->iparam[SHR_CHCNT]);
		dz->iparam[SHR_LAST_BUFLEN] = (dz->insams[0]%dz->buflen) /* /sizeof(short)*/;
		dz->iparam[SHR_LAST_CHCNT]  = round(lchunkcnt*((double)dz->iparam[SHR_LAST_BUFLEN]/(double)dz->buflen));
		dz->iparam[SHR_LAST_SCAT]   = min(dz->iparam[SHRED_SCAT],dz->iparam[SHR_LAST_CHCNT]);
	}
	if(dz->iparam[SHR_LAST_CHCNT] < 2) {
		fprintf(stdout, "WARNING: FINAL BUFFER WON'T BE SHREDDED (Too short for chunklen set).\n");
		fprintf(stdout, "WARNING: It will shred if you\n");
		fprintf(stdout, "WARNING: a) shorten infile by (>) chunklen, OR\n");
		fprintf(stdout, "WARNING: b) alter chunklen until last buffer has >1 chunk in it.\n");
		fflush(stdout);
	}
	memset((char *) (dz->sampbuf[0]),0,(dz->buflen * dz->bufcnt) * sizeof(float));


	return(FINISHED);
}

/***************************CREATE_BUFFERS **************************/

int create_scrub_buffers(dataptr dz)
{
	size_t bigbufsize;
	int shsecsize = F_SECSIZE, sum = 0L;
	int framesize = F_SECSIZE * dz->infile->channels;
    bigbufsize = (size_t) Malloc(-1);
	bigbufsize /= sizeof(float);
    bigbufsize = (bigbufsize/(framesize*dz->bufcnt)) * framesize * dz->bufcnt;
    bigbufsize = bigbufsize/dz->bufcnt;
	if(bigbufsize <= 0)
		bigbufsize  = framesize;
    dz->buflen = (int) bigbufsize;
    if((dz->bigbuf = (float *)malloc(  
		   ((dz->buflen * dz->bufcnt) + (F_SECSIZE*2)) * sizeof(float))) == NULL) {
		sprintf(errstr, "INSUFFICIENT MEMORY for sound buffers.\n");
		return(MEMORY_ERROR);
	}
    dz->sampbuf[0] = dz->bigbuf;
    dz->sampbuf[1] = dz->sampbuf[0] + dz->buflen + (shsecsize*2);	  /* inbuf has wraparound sectors */
	dz->iparam[SCRUB_DROPOUT] = FALSE;
    if(dz->iparam[SCRUB_TOTALDUR] <= dz->buflen + shsecsize) {
		dz->buflen = dz->iparam[SCRUB_TOTALDUR];
		dz->iparam[SCRUB_DROPOUT] = TRUE;
    }
	dz->iparam[SCRUB_BUFCNT] = 0;
	while(sum < dz->insams[0]) {
		dz->iparam[SCRUB_BUFCNT]++;
		sum += dz->buflen;
	}
	return(FINISHED);
}

/*************************** CREATE_CROSSMOD_BUFFERS **************************/
//TW replaced by global
//#define FSECSIZE (256)

int create_crossmod_buffers(dataptr dz)
{
	size_t bigbufsize;
	int framesize;
	int bufchunks;

	bufchunks  = dz->infile->channels;
	bufchunks += dz->otherfile->channels;
	bufchunks += max(dz->otherfile->channels,dz->infile->channels);
	bigbufsize = (size_t) Malloc(-1);
	dz->buflen = (int)(bigbufsize / sizeof(float));

//TW MODIFIED ....
	framesize = F_SECSIZE * bufchunks;
	if((dz->buflen  = (dz->buflen/framesize) * framesize)<=0)
		dz->buflen  = framesize;

	if((dz->bigbuf = (float *)malloc(dz->buflen * sizeof(float))) == NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY to create sound buffers.\n");
		return(PROGRAM_ERROR);
	}
	dz->buflen /= bufchunks;
	dz->sampbuf[0] = dz->bigbuf;
	dz->sampbuf[1] = dz->sampbuf[0] + (dz->buflen * dz->infile->channels);
	dz->sampbuf[2] = dz->sampbuf[1] + (dz->buflen * dz->otherfile->channels);
	return(FINISHED);
}

#########################

NEED ALIGNING TO CHANNELCNT * f_secsize

ap_extend.
create_seqbufs()
create_seqbufs2()

ap_filter.c
create_fltiter_buffer()

ap_grain.c
create_ssss_sndbufs()
create_rrr_sndbufs()

ap_hfperm.
create_hfpermbufs()

delay.c
create_delay_buffers()
create_stadium_delay()	Needs rewrite to give multichan reverb out

