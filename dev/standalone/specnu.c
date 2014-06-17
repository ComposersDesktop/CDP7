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
#include <pnames.h>
#include <filetype.h>
#include <processno.h>
#include <modeno.h>
#include <logic.h>
#include <globcon.h>
#include <cdpmain.h>
#include <math.h>
#include <mixxcon.h>
#include <osbind.h>
#include <standalone.h>
#include <speccon.h>
#include <ctype.h>
#include <sfsys.h>
#include <string.h>
#include <srates.h>

#if defined unix || defined __GNUC__
#define round(x) lround((x))
#endif
#ifndef HUGE
#define HUGE 3.40282347e+38F
#endif

char errstr[2400];

int anal_infiles = 1;
int	sloom = 0;
int sloombatch = 0;

const char* cdp_version = "6.1.0";

/* CDP LIBRARY FUNCTIONS TRANSFERRED HERE */

static int 	set_param_data(aplptr ap, int special_data,int maxparamcnt,int paramcnt,char *paramlist);
static int  set_vflgs(aplptr ap,char *optflags,int optcnt,char *optlist,
				char *varflags,int vflagcnt, int vparamcnt,char *varlist);
static int 	setup_parameter_storage_and_constants(int storage_cnt,dataptr dz);
static int	initialise_is_int_and_no_brk_constants(int storage_cnt,dataptr dz);
static int	mark_parameter_types(dataptr dz,aplptr ap);
static int  establish_application(dataptr dz);
static int  application_init(dataptr dz);
static int  initialise_vflags(dataptr dz);
static int  setup_input_param_defaultval_stores(int tipc,aplptr ap);
static int  setup_and_init_input_param_activity(dataptr dz,int tipc);
static int  get_tk_cmdline_word(int *cmdlinecnt,char ***cmdline,char *q);
static int  assign_file_data_storage(int infilecnt,dataptr dz);

/* CDP LIB FUNCTION MODIFIED TO AVOID CALLING setup_particular_application() */

static int  parse_sloom_data(int argc,char *argv[],char ***cmdline,int *cmdlinecnt,dataptr dz);

/* SIMPLIFICATION OF LIB FUNC TO APPLY TO JUST THIS FUNCTION */

static int  parse_infile_and_check_type(char **cmdline,dataptr dz);
static int	handle_the_extra_infile(char ***cmdline,int *cmdlinecnt,dataptr dz);
static int  handle_the_outfile(int *cmdlinecnt,char ***cmdline,int is_launched,dataptr dz);
static int  setup_the_application(dataptr dz);
static int  setup_the_param_ranges_and_defaults(dataptr dz);
static int	check_the_param_validity_and_consistency(dataptr dz);
static int  get_the_process_no(char *prog_identifier_from_cmdline,dataptr dz);
static int  setup_and_init_input_brktable_constants(dataptr dz,int brkcnt);
static int  get_the_mode_no(char *str, dataptr dz);

/* BYPASS LIBRARY GLOBAL FUNCTION TO GO DIRECTLY TO SPECIFIC APPLIC FUNCTIONS */

static int spec_remove(dataptr dz);
static int do_speclean(int subtract,dataptr dz);
static int allocate_speclean_buffer(dataptr dz);
static int handle_pitchdata(int *cmdlinecnt,char ***cmdline,dataptr dz);
static void insert_newnumber_at_filename_end(char *filename,int num,int overwrite_last_char);
static int create_next_outfile(dataptr dz);
static int do_specslice(dataptr dz);
static int setup_specslice_parameter_storage(dataptr dz);

/* SPECTOVF FUNCTIONS */

static int  spectovf(dataptr dz);
static int  specget(double time,double maxamp,dataptr dz);
static int  close_to_frq_already_in_ring(chvptr *there,double frq1,dataptr dz);
static int  substitute_in_ring(int vc,chvptr here,chvptr there,dataptr dz);
static int  insert_in_ring(int vc, chvptr here, dataptr dz);
static int  put_ring_frqs_in_ascending_order(chvptr **partials,float *minamp,dataptr dz);
static void find_pitch(chvptr *partials,double lo_loud_partial,double hi_loud_partial,float minamp,double time,float amp,dataptr dz);
static int  equivalent_pitches(double frq1, double frq2, dataptr dz);
static int  is_peak_at(double frq,int window_offset,float minamp,dataptr dz);
static int  enough_partials_are_harmonics(chvptr *partials,double pich_pich,dataptr dz);
static int  is_a_harmonic(double frq1,double frq2,dataptr dz);
static int  local_peak(int thiscc,double frq, float *thisbuf, dataptr dz);

/* SPECTOVF2 FUNCTIONS */

static int spectovf2(dataptr dz);
static int locate_peaks(int firstpass,double time,dataptr dz);
static int keep_peak(int firstpass,int vc,double time,dataptr dz);
static int remove_non_persisting_peaks(int firstpass,dataptr dz);
static int store_peaks(int *outcnt,double time,dataptr dz);
static void sort_peaks(int outcnt,dataptr dz);
static int write_peaks(int outcnt,dataptr dz);


#define peaktrail_cnt formant_bands

/**************************************** MAIN *********************************************/

int main(int argc,char *argv[])
{
	int exit_status;
	dataptr dz = NULL;
	char **cmdline;
	int  cmdlinecnt;
	aplptr ap;
	int is_launched = FALSE;
	if(argc==2 && (strcmp(argv[1],"--version") == 0)) {
		fprintf(stdout,"%s\n",cdp_version);
		fflush(stdout);
		return 0;
	}
						/* CHECK FOR SOUNDLOOM */
	if((sloom = sound_loom_in_use(&argc,&argv)) > 1) {
		sloom = 0;
		sloombatch = 1;
	}
	if(sflinit("cdp")){
		sfperror("cdp: initialisation\n");
		return(FAILED);
	}
						  /* SET UP THE PRINCIPLE DATASTRUCTURE */
	if((exit_status = establish_datastructure(&dz))<0) {					// CDP LIB
		print_messages_and_close_sndfiles(exit_status,is_launched,dz);
		return(FAILED);
	}
					  
	if(!sloom) {
		if(argc == 1) {
			usage1();	
			return(FAILED);
		} else if(argc == 2) {
			usage2(argv[1]);	
			return(FAILED);
		}
		if((exit_status = make_initial_cmdline_check(&argc,&argv))<0) {		// CDP LIB
			print_messages_and_close_sndfiles(exit_status,is_launched,dz);
			return(FAILED);
		}
		cmdline    = argv;
		cmdlinecnt = argc;
		if((get_the_process_no(argv[0],dz))<0)
			return(FAILED);
		cmdline++;
		cmdlinecnt--;
		switch(dz->process) {
		case(SPEC_REMOVE):	dz->maxmode = 2; break;
		case(SPECLEAN):		dz->maxmode = 0; break;
		case(SPECTRACT):	dz->maxmode = 0; break;
		case(SPECSLICE):	dz->maxmode = 4; break;
		case(SPECTOVF):		dz->maxmode = 0; break;
		case(SPECTOVF2):	dz->maxmode = 0; break;
		}
		if(dz->maxmode > 0) {
			if(cmdlinecnt <= 0) {
				sprintf(errstr,"Too few commandline parameters.\n");
				return(FAILED);
			}
			if((get_the_mode_no(cmdline[0],dz))<0) {
				if(!sloom)
					fprintf(stderr,"%s",errstr);
				return(FAILED);
			}
			cmdline++;
			cmdlinecnt--;
		}
		if((exit_status = setup_the_application(dz))<0) {
			print_messages_and_close_sndfiles(exit_status,is_launched,dz);
			return(FAILED);
		}
		if((exit_status = count_and_allocate_for_infiles(cmdlinecnt,cmdline,dz))<0) {		// CDP LIB
			print_messages_and_close_sndfiles(exit_status,is_launched,dz);
			return(FAILED);
		}
	} else {
		//parse_TK_data() =
		if((exit_status = parse_sloom_data(argc,argv,&cmdline,&cmdlinecnt,dz))<0) {
			exit_status = print_messages_and_close_sndfiles(exit_status,is_launched,dz);
			return(exit_status);		 
		}
	}
	ap = dz->application;

	// parse_infile_and_hone_type() = 
	if((exit_status = parse_infile_and_check_type(cmdline,dz))<0) {
		exit_status = print_messages_and_close_sndfiles(exit_status,is_launched,dz);
		return(FAILED);
	}
	// setup_param_ranges_and_defaults() =
	if((exit_status = setup_the_param_ranges_and_defaults(dz))<0) {
		exit_status = print_messages_and_close_sndfiles(exit_status,is_launched,dz);
		return(FAILED);
	}
	// open_first_infile		CDP LIB
	if((exit_status = open_first_infile(cmdline[0],dz))<0) {	
		print_messages_and_close_sndfiles(exit_status,is_launched,dz);	
		return(FAILED);
	}
	cmdlinecnt--;
	cmdline++;

//	handle_extra_infiles() =
	if(dz->process == SPECLEAN || dz->process == SPECTRACT) {
		if((exit_status = handle_the_extra_infile(&cmdline,&cmdlinecnt,dz))<0) {
			print_messages_and_close_sndfiles(exit_status,is_launched,dz);
			return(FAILED);
		}
	}
	// handle_outfile() = 
	if((exit_status = handle_the_outfile(&cmdlinecnt,&cmdline,is_launched,dz))<0) {
		print_messages_and_close_sndfiles(exit_status,is_launched,dz);
		return(FAILED);
	}

//	handle_formants()			redundant
//	handle_formant_quiksearch()	redundant
//	handle_special_data()		redundant except
	if(dz->process == SPECSLICE && dz->mode == 3) {
		if((exit_status = handle_pitchdata(&cmdlinecnt,&cmdline,dz))<0) {
			print_messages_and_close_sndfiles(exit_status,is_launched,dz);
			return(FAILED);
		}
	} 
	if((exit_status = read_parameters_and_flags(&cmdline,&cmdlinecnt,dz))<0) {		// CDP LIB
		print_messages_and_close_sndfiles(exit_status,is_launched,dz);
		return(FAILED);
	}
	//check_param_validity_and_consistency .....
	if((exit_status = check_the_param_validity_and_consistency(dz))<0) {
		print_messages_and_close_sndfiles(exit_status,is_launched,dz);
		return(FAILED);
	}
	is_launched = TRUE;

	//allocate_large_buffers() ... replaced by	CDP LIB
	switch(dz->process) {
	case(SPEC_REMOVE):	dz->extra_bufcnt =  0;	dz->bptrcnt = 1;				break;
	case(SPECTRACT):
	case(SPECLEAN):		dz->extra_bufcnt =  0;	dz->bptrcnt = dz->iparam[0];	break;
	case(SPECSLICE):	dz->extra_bufcnt =  0;	dz->bptrcnt = 0;				break;
	case(SPECTOVF2):
	case(SPECTOVF):		dz->extra_bufcnt =  3;	dz->bptrcnt = 2; 				break;

	}
	if((exit_status = establish_spec_bufptrs_and_extra_buffers(dz))<0) {
		print_messages_and_close_sndfiles(exit_status,is_launched,dz);
		return(FAILED);
	}
	switch(dz->process) {
	case(SPECTOVF):
	case(SPECTOVF2):
	case(SPEC_REMOVE):	exit_status = allocate_single_buffer(dz);	break;
	case(SPECSLICE):
	case(SPECTRACT):
	case(SPECLEAN):		exit_status = allocate_speclean_buffer(dz);	break;
	}
	if(exit_status < 0) {
		print_messages_and_close_sndfiles(exit_status,is_launched,dz);
		return(FAILED);
	}
	
	//param_preprocess()						redundant
	//spec_process_file =
	switch(dz->process) {
	case(SPEC_REMOVE):	exit_status = outer_loop(dz);		break;
	case(SPECLEAN):		exit_status = do_speclean(0,dz);	break;
	case(SPECTRACT):	exit_status = do_speclean(1,dz);	break;
	case(SPECSLICE):	exit_status = do_specslice(dz);		break;
	case(SPECTOVF):		exit_status = spectovf(dz);			break;
	case(SPECTOVF2):	exit_status = spectovf2(dz);		break;
	}
	if(exit_status < 0) {
		print_messages_and_close_sndfiles(exit_status,is_launched,dz);
		return(FAILED);
	}

	if((exit_status = complete_output(dz))<0) {										// CDP LIB
		print_messages_and_close_sndfiles(exit_status,is_launched,dz);
		return(FAILED);
	}
	exit_status = print_messages_and_close_sndfiles(FINISHED,is_launched,dz);		// CDP LIB
	free(dz);
	return(SUCCEEDED);
}

/**********************************************
		REPLACED CDP LIB FUNCTIONS
**********************************************/

/************************ HANDLE_THE_EXTRA_INFILE *********************/

int handle_the_extra_infile(char ***cmdline,int *cmdlinecnt,dataptr dz)
{
					/* OPEN ONE EXTRA ANALFILE, CHECK COMPATIBILITY */
	int  exit_status;
	char *filename;
	fileptr fp2;
	int fileno = 1;
	double maxamp, maxloc;
	int maxrep;
	int getmax = 0, getmaxinfo = 0;
	infileptr ifp;
	fileptr fp1 = dz->infile; 
	filename = (*cmdline)[0];
	if((dz->ifd[fileno] = sndopenEx(filename,0,CDP_OPEN_RDONLY)) < 0) {
		sprintf(errstr,"cannot open input file %s to read data.\n",filename);
		return(DATA_ERROR);
	}	
	if((ifp = (infileptr)malloc(sizeof(struct filedata)))==NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY to store data on later infile. (1)\n");
		return(MEMORY_ERROR);
	}
	if((fp2 = (fileptr)malloc(sizeof(struct fileprops)))==NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY to store data on later infile. (2)\n");
		return(MEMORY_ERROR);
	}
	if((exit_status = readhead(ifp,dz->ifd[1],filename,&maxamp,&maxloc,&maxrep,getmax,getmaxinfo))<0)
		return(exit_status);
	copy_to_fileptr(ifp,fp2);
	if(fp2->filetype != ANALFILE) {
		sprintf(errstr,"%s is not an analysis file.\n",filename);
		return(DATA_ERROR);
	}
	if(fp2->origstype != fp1->origstype) {
		sprintf(errstr,"Incompatible original-sample-type in input file %s.\n",filename);
		return(DATA_ERROR);
	}
	if(fp2->origrate != fp1->origrate) {
		sprintf(errstr,"Incompatible original-sample-rate in input file %s.\n",filename);
		return(DATA_ERROR);
	}
	if(fp2->arate != fp1->arate) {
		sprintf(errstr,"Incompatible analysis-sample-rate in input file %s.\n",filename);
		return(DATA_ERROR);
	}
	if(fp2->Mlen != fp1->Mlen) {
		sprintf(errstr,"Incompatible analysis-window-length in input file %s.\n",filename);
		return(DATA_ERROR);
	}
	if(fp2->Dfac != fp1->Dfac) {
		sprintf(errstr,"Incompatible decimation factor in input file %s.\n",filename);
		return(DATA_ERROR);
	}
	if(fp2->channels != fp1->channels) {
		sprintf(errstr,"Incompatible channel-count in input file %s.\n",filename);
		return(DATA_ERROR);
	}
	if((dz->insams[fileno] = sndsizeEx(dz->ifd[fileno]))<0) {	    /* FIND SIZE OF FILE */
		sprintf(errstr, "Can't read size of input file %s.\n"
		"open_checktype_getsize_and_compareheader()\n",filename);
		return(PROGRAM_ERROR);
	}
	if(dz->insams[fileno]==0) {
		sprintf(errstr, "File %s contains no data.\n",filename);
		return(DATA_ERROR);
	}
	(*cmdline)++;
	(*cmdlinecnt)--;
	return(FINISHED);
}

/****************************** SET_PARAM_DATA *********************************/

int set_param_data(aplptr ap, int special_data,int maxparamcnt,int paramcnt,char *paramlist)
{
	ap->special_data   = (char)special_data;	   
	ap->param_cnt      = (char)paramcnt;
	ap->max_param_cnt  = (char)maxparamcnt;
	if(ap->max_param_cnt>0) {
		if((ap->param_list = (char *)malloc((size_t)(ap->max_param_cnt+1)))==NULL) {	
			sprintf(errstr,"INSUFFICIENT MEMORY: for param_list\n");
			return(MEMORY_ERROR);
		}
		strcpy(ap->param_list,paramlist); 
	}
	return(FINISHED);
}

/****************************** SET_VFLGS *********************************/

int set_vflgs
(aplptr ap,char *optflags,int optcnt,char *optlist,char *varflags,int vflagcnt, int vparamcnt,char *varlist)
{
	ap->option_cnt 	 = (char) optcnt;			/*RWD added cast */
	if(optcnt) {
		if((ap->option_list = (char *)malloc((size_t)(optcnt+1)))==NULL) {
			sprintf(errstr,"INSUFFICIENT MEMORY: for option_list\n");
			return(MEMORY_ERROR);
		}
		strcpy(ap->option_list,optlist);
		if((ap->option_flags = (char *)malloc((size_t)(optcnt+1)))==NULL) {
			sprintf(errstr,"INSUFFICIENT MEMORY: for option_flags\n");
			return(MEMORY_ERROR);
		}
		strcpy(ap->option_flags,optflags); 
	}
	ap->vflag_cnt = (char) vflagcnt;		   
	ap->variant_param_cnt = (char) vparamcnt;
	if(vflagcnt) {
		if((ap->variant_list  = (char *)malloc((size_t)(vflagcnt+1)))==NULL) {
			sprintf(errstr,"INSUFFICIENT MEMORY: for variant_list\n");
			return(MEMORY_ERROR);
		}
		strcpy(ap->variant_list,varlist);		
		if((ap->variant_flags = (char *)malloc((size_t)(vflagcnt+1)))==NULL) {
			sprintf(errstr,"INSUFFICIENT MEMORY: for variant_flags\n");
			return(MEMORY_ERROR);
		}
		strcpy(ap->variant_flags,varflags);

	}
	return(FINISHED);
}

/***************************** APPLICATION_INIT **************************/

int application_init(dataptr dz)
{
	int exit_status;
	int storage_cnt;
	int tipc, brkcnt;
	aplptr ap = dz->application;
	if(ap->vflag_cnt>0)
		initialise_vflags(dz);	  
	tipc  = ap->max_param_cnt + ap->option_cnt + ap->variant_param_cnt;
	ap->total_input_param_cnt = (char)tipc;
	if(tipc>0) {
		if((exit_status = setup_input_param_range_stores(tipc,ap))<0)			  
			return(exit_status);
		if((exit_status = setup_input_param_defaultval_stores(tipc,ap))<0)		  
			return(exit_status);
		if((exit_status = setup_and_init_input_param_activity(dz,tipc))<0)	  
			return(exit_status);
	}
	brkcnt = tipc;
	if(brkcnt>0) {
		if((exit_status = setup_and_init_input_brktable_constants(dz,brkcnt))<0)			  
			return(exit_status);
	}
	if((storage_cnt = tipc + ap->internal_param_cnt)>0) {		  
		if((exit_status = setup_parameter_storage_and_constants(storage_cnt,dz))<0)	  
			return(exit_status);
		if((exit_status = initialise_is_int_and_no_brk_constants(storage_cnt,dz))<0)	  
			return(exit_status);
	}													   
 	if((exit_status = mark_parameter_types(dz,ap))<0)	  
			return(exit_status);
	
	// establish_infile_constants() replaced by
	dz->infilecnt = ONE_NONSND_FILE;
	//establish_bufptrs_and_extra_buffers():
	if(dz->process == SPECSLICE && dz->mode == 3) {
		if((exit_status = setup_specslice_parameter_storage(dz))<0)	  
			return(exit_status);
	} else {
		dz->array_cnt=2;
		if((dz->parray  = (double **)malloc(dz->array_cnt * sizeof(double *)))==NULL) {
			sprintf(errstr,"INSUFFICIENT MEMORY for internal double arrays.\n");
			return(MEMORY_ERROR);
		}
		dz->parray[0] = NULL;
		dz->parray[1] = NULL;
	}
	return(FINISHED);
}

/******************************** SETUP_AND_INIT_INPUT_BRKTABLE_CONSTANTS ********************************/

int setup_and_init_input_brktable_constants(dataptr dz,int brkcnt)
{	
	int n;
	if((dz->brk      = (double **)malloc(brkcnt * sizeof(double *)))==NULL) {
		sprintf(errstr,"setup_and_init_input_brktable_constants(): 1\n");
		return(MEMORY_ERROR);
	}
	if((dz->brkptr   = (double **)malloc(brkcnt * sizeof(double *)))==NULL) {
		sprintf(errstr,"setup_and_init_input_brktable_constants(): 6\n");
		return(MEMORY_ERROR);
	}
	if((dz->brksize  = (int    *)malloc(brkcnt * sizeof(int)))==NULL) {
		sprintf(errstr,"setup_and_init_input_brktable_constants(): 2\n");
		return(MEMORY_ERROR);
	}
	if((dz->firstval = (double  *)malloc(brkcnt * sizeof(double)))==NULL) {
		sprintf(errstr,"setup_and_init_input_brktable_constants(): 3\n");
		return(MEMORY_ERROR);												  
	}
	if((dz->lastind  = (double  *)malloc(brkcnt * sizeof(double)))==NULL) {
		sprintf(errstr,"setup_and_init_input_brktable_constants(): 4\n");
		return(MEMORY_ERROR);
	}
	if((dz->lastval  = (double  *)malloc(brkcnt * sizeof(double)))==NULL) {
		sprintf(errstr,"setup_and_init_input_brktable_constants(): 5\n");
		return(MEMORY_ERROR);
	}
	if((dz->brkinit  = (int     *)malloc(brkcnt * sizeof(int)))==NULL) {
		sprintf(errstr,"setup_and_init_input_brktable_constants(): 7\n");
		return(MEMORY_ERROR);
	}
	for(n=0;n<brkcnt;n++) {
		dz->brk[n]     = NULL;
		dz->brkptr[n]  = NULL;
		dz->brkinit[n] = 0;
		dz->brksize[n] = 0;
	}
	return(FINISHED);
}

/********************** SETUP_PARAMETER_STORAGE_AND_CONSTANTS ********************/
/* RWD mallo changed to calloc; helps debug verison run as release! */

int setup_parameter_storage_and_constants(int storage_cnt,dataptr dz)
{
	if((dz->param       = (double *)calloc(storage_cnt, sizeof(double)))==NULL) {
		sprintf(errstr,"setup_parameter_storage_and_constants(): 1\n");
		return(MEMORY_ERROR);
	}
	if((dz->iparam      = (int    *)calloc(storage_cnt, sizeof(int)   ))==NULL) {
		sprintf(errstr,"setup_parameter_storage_and_constants(): 2\n");
		return(MEMORY_ERROR);
	}
	if((dz->is_int      = (char   *)calloc(storage_cnt, sizeof(char)))==NULL) {
		sprintf(errstr,"setup_parameter_storage_and_constants(): 3\n");
		return(MEMORY_ERROR);
	}
	if((dz->no_brk      = (char   *)calloc(storage_cnt, sizeof(char)))==NULL) {
		sprintf(errstr,"setup_parameter_storage_and_constants(): 5\n");
		return(MEMORY_ERROR);
	}
	return(FINISHED);
}

/************** INITIALISE_IS_INT_AND_NO_BRK_CONSTANTS *****************/

int initialise_is_int_and_no_brk_constants(int storage_cnt,dataptr dz)
{
	int n;
	for(n=0;n<storage_cnt;n++) {
		dz->is_int[n] = (char)0;
		dz->no_brk[n] = (char)0;
	}
	return(FINISHED);
}

/***************************** MARK_PARAMETER_TYPES **************************/

int mark_parameter_types(dataptr dz,aplptr ap)
{
	int n, m;							/* PARAMS */
	for(n=0;n<ap->max_param_cnt;n++) {
		switch(ap->param_list[n]) {
		case('0'):	break; /* dz->is_active[n] = 0 is default */
		case('i'):	dz->is_active[n] = (char)1; dz->is_int[n] = (char)1;dz->no_brk[n] = (char)1; break;
		case('I'):	dz->is_active[n] = (char)1;	dz->is_int[n] = (char)1; 						 break;
		case('d'):	dz->is_active[n] = (char)1;							dz->no_brk[n] = (char)1; break;
		case('D'):	dz->is_active[n] = (char)1;	/* normal case: double val or brkpnt file */	 break;
		default:
			sprintf(errstr,"Programming error: invalid parameter type in mark_parameter_types()\n");
			return(PROGRAM_ERROR);
		}
	}						 		/* OPTIONS */
	for(n=0,m=ap->max_param_cnt;n<ap->option_cnt;n++,m++) {
		switch(ap->option_list[n]) {
		case('i'): dz->is_active[m] = (char)1; dz->is_int[m] = (char)1;	dz->no_brk[m] = (char)1; break;
		case('I'): dz->is_active[m] = (char)1; dz->is_int[m] = (char)1; 						 break;
		case('d'): dz->is_active[m] = (char)1; 							dz->no_brk[m] = (char)1; break;
		case('D'): dz->is_active[m] = (char)1;	/* normal case: double val or brkpnt file */	 break;
		default:
			sprintf(errstr,"Programming error: invalid option type in mark_parameter_types()\n");
			return(PROGRAM_ERROR);
		}
	}								/* VARIANTS */
	for(n=0,m=ap->max_param_cnt + ap->option_cnt;n < ap->variant_param_cnt; n++, m++) {
		switch(ap->variant_list[n]) {
		case('0'): break;
		case('i'): dz->is_active[m] = (char)1; dz->is_int[m] = (char)1;	dz->no_brk[m] = (char)1; break;
		case('I'): dz->is_active[m] = (char)1; dz->is_int[m] = (char)1;	 						 break;
		case('d'): dz->is_active[m] = (char)1; 							dz->no_brk[m] = (char)1; break;
		case('D'): dz->is_active[m] = (char)1; /* normal case: double val or brkpnt file */		 break;
		default:
			sprintf(errstr,"Programming error: invalid variant type in mark_parameter_types()\n");
			return(PROGRAM_ERROR);
		}
	}								/* INTERNAL */
	for(n=0,
	m=ap->max_param_cnt + ap->option_cnt + ap->variant_param_cnt; n<ap->internal_param_cnt; n++,m++) {
		switch(ap->internal_param_list[n]) {
		case('0'):  break;	 /* dummy variables: variables not used: but important for internal paream numbering!! */
		case('i'):	dz->is_int[m] = (char)1;	dz->no_brk[m] = (char)1;	break;
		case('d'):								dz->no_brk[m] = (char)1;	break;
		default:
			sprintf(errstr,"Programming error: invalid internal param type in mark_parameter_types()\n");
			return(PROGRAM_ERROR);
		}
	}
	return(FINISHED);
}

/***************************** HANDLE_THE_OUTFILE **************************/

int handle_the_outfile(int *cmdlinecnt,char ***cmdline,int is_launched,dataptr dz)
{
	int exit_status, len;
	char *filename = NULL, *p;
	if(dz->process == SPECSLICE) {
		if(dz->wordstor!=NULL)
			free_wordstors(dz);
		dz->all_words = 0;
		if((exit_status = store_filename((*cmdline)[0],dz))<0)
			return(exit_status);
		len = strlen((*cmdline)[0]);
		if((filename = (char *)malloc(len + 2))==NULL) {
			sprintf(errstr,"handle_the_outfile()\n");
			return(MEMORY_ERROR);
		}
		strcpy(filename,dz->wordstor[0]);
		dz->itemcnt = 0;
	} else if(dz->process == SPECTOVF || dz->process == SPECTOVF2) {
		len = strlen((*cmdline)[0]);
		if((filename = (char *)malloc(len + 10))==NULL) {
			sprintf(errstr,"handle_the_outfile()\n");
			return(MEMORY_ERROR);
		}
		strcpy(filename,(*cmdline)[0]);
		p = filename + len;
		p--;
		while(*p != '.') {
			p--;
			if(p == filename) {
				p = filename + len;
				break;
			}
		}
		*p = ENDOFSTR;
		strcat(filename,".txt");
	} else {
		filename = (*cmdline)[0];
	}
	strcpy(dz->outfilename,filename);	   
	if((exit_status = create_sized_outfile(filename,dz))<0)
		return(exit_status);
	(*cmdline)++;
	(*cmdlinecnt)--;
	return(FINISHED);
}

/***************************** CREATE_NEXT_OUTFILE **************************/

int create_next_outfile(dataptr dz)
{
	int exit_status, len;
	char *filename = NULL;
	dz->itemcnt++;
	len = strlen(dz->wordstor[0]) + 5;
	if((filename = (char *)malloc(len))==NULL) {
		sprintf(errstr,"handle_the_outfile()\n");
		return(MEMORY_ERROR);
	}
	strcpy(filename,dz->wordstor[0]);
	insert_newnumber_at_filename_end(filename,dz->itemcnt,1);
	strcpy(dz->outfilename,filename);	   
	if((exit_status = create_sized_outfile(filename,dz))<0)
		return(exit_status);
	return(FINISHED);
}

/***************************** INSERT_NEWNUMBER_AT_FILENAME_END **************************/

void insert_newnumber_at_filename_end(char *filename,int num,int overwrite_last_char)
/* FUNCTIONS ASSUMES ENOUGH SPACE IS ALLOCATED !! */
{
	char *p;
	char ext[64];
	p = filename + strlen(filename) - 1;
	while(p > filename) {
		if(*p == '/' || *p == '\\' || *p == ':') {
			p = filename;
			break;
		}
		if(*p == '.') {
			strcpy(ext,p);
			if(overwrite_last_char)
				p--;
			sprintf(p,"%d",num);
			strcat(filename,ext);
			return;
		}
		p--;
	}
	if(p == filename) {
		p += strlen(filename);
		if(overwrite_last_char)
			p--;
		sprintf(p,"%d",num);
	}
}

/***************************** ESTABLISH_APPLICATION **************************/

int establish_application(dataptr dz)
{
	aplptr ap;
	if((dz->application = (aplptr)malloc(sizeof (struct applic)))==NULL) {
		sprintf(errstr,"establish_application()\n");
		return(MEMORY_ERROR);
	}
	ap = dz->application;
	memset((char *)ap,0,sizeof(struct applic));
	return(FINISHED);
}

/************************* INITIALISE_VFLAGS *************************/

int initialise_vflags(dataptr dz)
{
	int n;
	if((dz->vflag  = (char *)malloc(dz->application->vflag_cnt * sizeof(char)))==NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY: vflag store,\n");
		return(MEMORY_ERROR);
	}
	for(n=0;n<dz->application->vflag_cnt;n++)
		dz->vflag[n]  = FALSE;
	return FINISHED;
}

/************************* SETUP_INPUT_PARAM_DEFAULTVALS *************************/

int setup_input_param_defaultval_stores(int tipc,aplptr ap)
{
	int n;
	if((ap->default_val = (double *)malloc(tipc * sizeof(double)))==NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY for application default values store\n");
		return(MEMORY_ERROR);
	}
	for(n=0;n<tipc;n++)
		ap->default_val[n] = 0.0;
	return(FINISHED);
}

/***************************** SETUP_AND_INIT_INPUT_PARAM_ACTIVITY **************************/

int setup_and_init_input_param_activity(dataptr dz,int tipc)
{
	int n;
	if((dz->is_active = (char   *)malloc((size_t)tipc))==NULL) {
		sprintf(errstr,"setup_and_init_input_param_activity()\n");
		return(MEMORY_ERROR);
	}
	for(n=0;n<tipc;n++)
		dz->is_active[n] = (char)0;
	return(FINISHED);
}

/************************* SETUP_THE_APPLICATION *******************/

int setup_the_application(dataptr dz)
{
	int exit_status;
	aplptr ap;
	if((exit_status = establish_application(dz))<0)		// GLOBAL
		return(FAILED);
	ap = dz->application;
	// SEE parstruct FOR EXPLANATION of next 2 functions
	switch(dz->process) {
	case(SPEC_REMOVE):
		if((exit_status = set_param_data(ap,0   ,4,4,"dddD"      ))<0)
			return(FAILED);
		if((exit_status = set_vflgs(ap,"",0,"","",0,0,""))<0)
			return(FAILED);
		// set_legal_infile_structure -->
		dz->has_otherfile = FALSE;
		// assign_process_logic -->
		dz->input_data_type = ANALFILE_ONLY;
		dz->process_type	= EQUAL_ANALFILE;	
		dz->outfiletype  	= ANALFILE_OUT;
		break;
	case(SPECTRACT):
	case(SPECLEAN):
		if((exit_status = set_param_data(ap,0   ,2,2,"dd"      ))<0)
			return(FAILED);
		if((exit_status = set_vflgs(ap,"",0,"","",0,0,""))<0)
			return(FAILED);
		// set_legal_infile_structure -->
		dz->has_otherfile = FALSE;
		// assign_process_logic -->
		dz->input_data_type = TWO_ANALFILES;
		dz->process_type	= EQUAL_ANALFILE;	
		dz->outfiletype  	= ANALFILE_OUT;
		break;
	case(SPECSLICE):
		switch(dz->mode) {
		case(0):
			if((exit_status = set_param_data(ap,0   ,2,2,"iI"      ))<0)
				return(FAILED);
			break;
		case(1):
		case(2):
			if((exit_status = set_param_data(ap,0   ,2,2,"iD"      ))<0)
				return(FAILED);
			break;
		case(3):
			if((exit_status = set_param_data(ap,P_BRK_DATA   ,0,0,""        ))<0)
				return(FAILED);
			break;
		}
		if((exit_status = set_vflgs(ap,"",0,"","",0,0,""))<0)
			return(FAILED);
		// set_legal_infile_structure -->
		dz->has_otherfile = FALSE;
		// assign_process_logic -->
		dz->input_data_type = ANALFILE_ONLY;
		dz->process_type	= EQUAL_ANALFILE;	
		dz->outfiletype  	= ANALFILE_OUT;
		break;
	case(SPECTOVF):
		if((exit_status = set_param_data(ap,0   ,4,4,"didd"))<0)
			return(FAILED);
		if((exit_status = set_vflgs(ap,"",0,"","",0,0,""))<0)
			return(FAILED);
		// set_legal_infile_structure -->
		dz->has_otherfile = FALSE;
		// assign_process_logic -->
		dz->input_data_type = ANALFILE_ONLY;
		dz->process_type	= TO_TEXTFILE;	
		dz->outfiletype  	= TEXTFILE_OUT;
		break;
	case(SPECTOVF2):
		if((exit_status = set_param_data(ap,0   ,7,7,"ddddddd"))<0)
			return(FAILED);
		if((exit_status = set_vflgs(ap,"lw",2,"dd","an",2,0,"0"))<0)
			return(FAILED);
		// set_legal_infile_structure -->
		dz->has_otherfile = FALSE;
		// assign_process_logic -->
		dz->input_data_type = ANALFILE_ONLY;
		dz->process_type	= TO_TEXTFILE;	
		dz->outfiletype  	= TEXTFILE_OUT;
		break;
	}
	return application_init(dz);	//GLOBAL
}

/************************* PARSE_INFILE_AND_CHECK_TYPE *******************/

int parse_infile_and_check_type(char **cmdline,dataptr dz)
{
	int exit_status;
	infileptr infile_info;
	if(!sloom) {
		if((infile_info = (infileptr)malloc(sizeof(struct filedata)))==NULL) {
			sprintf(errstr,"INSUFFICIENT MEMORY for infile structure to test file data.");
			return(MEMORY_ERROR);
		} else if((exit_status = cdparse(cmdline[0],infile_info))<0) {
			sprintf(errstr,"Failed tp parse input file %s\n",cmdline[0]);
			return(PROGRAM_ERROR);
		} else if(infile_info->filetype != ANALFILE)  {
			sprintf(errstr,"File %s is not of correct type\n",cmdline[0]);
			return(DATA_ERROR);
		} else if((exit_status = copy_parse_info_to_main_structure(infile_info,dz))<0) {
			sprintf(errstr,"Failed to copy file parsing information\n");
			return(PROGRAM_ERROR);
		}
		free(infile_info);
	}
	dz->clength		= dz->wanted / 2;
	dz->chwidth 	= dz->nyquist/(double)(dz->clength-1);
	dz->halfchwidth = dz->chwidth/2.0;
	return(FINISHED);
}

/************************* SETUP_THE_PARAM_RANGES_AND_DEFAULTS *******************/

int setup_the_param_ranges_and_defaults(dataptr dz)
{
	int exit_status;
	aplptr ap = dz->application;
	// set_param_ranges()
	ap->total_input_param_cnt = (char)(ap->max_param_cnt + ap->option_cnt + ap->variant_param_cnt);
	// NB total_input_param_cnt is > 0 !!!s
	if((exit_status = setup_input_param_range_stores(ap->total_input_param_cnt,ap))<0)
		return(FAILED);
	// get_param_ranges()
	switch(dz->process) {
	case(SPEC_REMOVE):
		ap->lo[0]			= MIDIMIN;
		ap->hi[0]			= MIDIMAX;
		ap->default_val[0]	= 60;
		ap->lo[1]			= MIDIMIN;
		ap->hi[1]			= MIDIMAX;
		ap->default_val[1]	= 60;
		ap->lo[2]			= 0;
		ap->hi[2]			= dz->nyquist;
		ap->default_val[2]	= 6000.0;
		ap->lo[3]			= 0.0;
		ap->hi[3]			= 1.0;
		ap->default_val[3]	= 1.0;
		dz->maxmode = 2;
		break;
	case(SPECLEAN):
	case(SPECTRACT):
		ap->lo[0]			= 0.0;
		ap->hi[0]			= 1000.0;
		ap->default_val[0]	= dz->frametime * 8.0 * SECS_TO_MS;
		ap->lo[1]    		= 1.0;
		ap->hi[1]    		= CL_MAX_GAIN;
		ap->default_val[1]	= DEFAULT_NOISEGAIN;
		dz->maxmode = 0;
		break;
	case(SPECSLICE):
		switch(dz->mode) {
		case(0):
			ap->lo[0]			= 2;
			ap->hi[0]			= dz->clength/2;
			ap->default_val[0]	= 2;
			ap->lo[1]    		= 1;
			ap->hi[1]    		= dz->clength/2;
			ap->default_val[1]	= 1;
			break;
		case(1):
			ap->lo[0]			= 2;
			ap->hi[0]			= dz->clength/2;
			ap->default_val[0]	= 2;
			ap->lo[1]    		= dz->chwidth;
			ap->hi[1]    		= dz->nyquist/2.0;
			ap->default_val[1]	= dz->chwidth;
			break;
		case(2):
			ap->lo[0]			= 2;
			ap->hi[0]			= dz->clength/2;
			ap->default_val[0]	= 2;
			ap->lo[1]    		= 0.5;
			ap->hi[1]    		= (LOG2(dz->nyquist/dz->chwidth)/2.0) * SEMITONES_PER_OCTAVE;
			ap->default_val[1]	= SEMITONES_PER_OCTAVE;
			break;
		}
		dz->maxmode = 3;
		break;
	case(SPECTOVF):
		ap->lo[SVF_RANGE]   		 = 0.0;
		ap->hi[SVF_RANGE]   		 = 6.0;
		ap->default_val[SVF_RANGE]   = 1.0;
		ap->lo[SVF_MATCH]  		 = 1.0;
		ap->hi[SVF_MATCH]  		 = (double)MAXIMI;
		ap->default_val[SVF_MATCH]  = (double)ACCEPTABLE_MATCH;
		ap->lo[SVF_HILM]   		 = SPEC_MINFRQ;
		ap->hi[SVF_HILM]   		 = dz->nyquist/MAXIMI;
		ap->default_val[SVF_HILM]   = dz->nyquist/MAXIMI;
		ap->lo[SVF_LOLM]   		 = SPEC_MINFRQ;
		ap->hi[SVF_LOLM]   		 = dz->nyquist/MAXIMI;
		ap->default_val[SVF_LOLM]   = SPEC_MINFRQ;
		dz->maxmode = 0;
		break;
	case(SPECTOVF2):
		ap->lo[SVF2_HILM]			= SPEC_MINFRQ;		// maxfrq to look for peaks
		ap->hi[SVF2_HILM]   		= dz->nyquist;
		ap->default_val[SVF2_HILM]  = ap->hi[0];
		ap->lo[SVF2_PKNG]   		= 1.0;				// how much louder than average-or-median must peak be, to be 'true' peak
		ap->hi[SVF2_PKNG]   		= 100.0;
		ap->default_val[SVF2_PKNG]  = 2.0;
		ap->lo[SVF2_CTOF]   		= 0.0;				// relative cutoff amplitude (relative to window maxpeak)
		ap->hi[SVF2_CTOF]   		= 1.0;
		ap->default_val[SVF2_CTOF]  = .01;
		ap->lo[SVF2_WNDR]  			= 0;			    // semitone range that peak can wander and remain SAME peak-trail
		ap->hi[SVF2_WNDR]  			= 12.0;
		ap->default_val[SVF2_WNDR]  = 1.0;
		ap->lo[SVF2_PSST]   		= dz->frametime * SECS_TO_MS;	// Duration (mS) for which peak-trail must persist, to be 'true' peak-trail.	
		ap->hi[SVF2_PSST]   		= 1.0 * SECS_TO_MS;
		ap->default_val[SVF2_PSST]  = 4 * SECS_TO_MS;
		ap->lo[SVF2_TSTP]   		= dz->frametime * SECS_TO_MS;
		ap->hi[SVF2_TSTP]   		= 1.0 * SECS_TO_MS;	//	Minimum timestep (mS) between data outputs to filter file.
		ap->default_val[SVF2_TSTP]  = 4 * SECS_TO_MS;
		ap->lo[SVF2_SGNF]   		= 0;				//	semitone shift any of output peak must take before new filter val output.
		ap->hi[SVF2_SGNF]   		= 12.0;
		ap->default_val[SVF2_SGNF]  = 0.5;
		ap->lo[SVF2_LOLM]   		= SPEC_MINFRQ;		// minfrq to look for peaks
		ap->hi[SVF2_LOLM]   		= dz->nyquist;
		ap->default_val[SVF2_LOLM]  = SPEC_MINFRQ;
		ap->lo[SVF2_WSIZ]   		= 1.0;		// windowsize (semitones) used to locate peaks
		ap->hi[SVF2_WSIZ]   		= round(ceil(dz->nyquist/SPEC_MINFRQ)) * SEMITONES_PER_OCTAVE;
		ap->default_val[SVF2_WSIZ]  = 12.0;
		dz->maxmode = 0;
		break;
	}
	if(!sloom)
		put_default_vals_in_all_params(dz);
	return(FINISHED);
}

/********************************* PARSE_SLOOM_DATA *********************************/

int parse_sloom_data(int argc,char *argv[],char ***cmdline,int *cmdlinecnt,dataptr dz)
{
	int exit_status;
	int cnt = 1, infilecnt;
	int filesize, insams, inbrksize;
	double dummy;
	int true_cnt = 0;
	aplptr ap;

	while(cnt<=PRE_CMDLINE_DATACNT) {
		if(cnt > argc) {
			sprintf(errstr,"Insufficient data sent from TK\n");
			return(DATA_ERROR);
		}
		switch(cnt) {
		case(1):	
			if(sscanf(argv[cnt],"%d",&dz->process)!=1) {
				sprintf(errstr,"Cannot read process no. sent from TK\n");
				return(DATA_ERROR);
			}
			break;

		case(2):	
			if(sscanf(argv[cnt],"%d",&dz->mode)!=1) {
				sprintf(errstr,"Cannot read mode no. sent from TK\n");
				return(DATA_ERROR);
			}
			if(dz->mode > 0)
				dz->mode--;
			//setup_particular_application() =
			if((exit_status = setup_the_application(dz))<0)
				return(exit_status);
			ap = dz->application;
			break;

		case(3):	
			if(sscanf(argv[cnt],"%d",&infilecnt)!=1) {
				sprintf(errstr,"Cannot read infilecnt sent from TK\n");
				return(DATA_ERROR);
			}
			if(infilecnt < 1) {
				true_cnt = cnt + 1;
				cnt = PRE_CMDLINE_DATACNT;	/* force exit from loop after assign_file_data_storage */
			}
			if((exit_status = assign_file_data_storage(infilecnt,dz))<0)
				return(exit_status);
			break;
		case(INPUT_FILETYPE+4):	
			if(sscanf(argv[cnt],"%d",&dz->infile->filetype)!=1) {
				sprintf(errstr,"Cannot read filetype sent from TK (%s)\n",argv[cnt]);
				return(DATA_ERROR);
			}
			break;
		case(INPUT_FILESIZE+4):	
			if(sscanf(argv[cnt],"%d",&filesize)!=1) {
				sprintf(errstr,"Cannot read infilesize sent from TK\n");
				return(DATA_ERROR);
			}
			dz->insams[0] = filesize;	
			break;
		case(INPUT_INSAMS+4):	
			if(sscanf(argv[cnt],"%d",&insams)!=1) {
				sprintf(errstr,"Cannot read insams sent from TK\n");
				return(DATA_ERROR);
			}
			dz->insams[0] = insams;	
			break;
		case(INPUT_SRATE+4):	
			if(sscanf(argv[cnt],"%d",&dz->infile->srate)!=1) {
				sprintf(errstr,"Cannot read srate sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_CHANNELS+4):	
			if(sscanf(argv[cnt],"%d",&dz->infile->channels)!=1) {
				sprintf(errstr,"Cannot read channels sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_STYPE+4):	
			if(sscanf(argv[cnt],"%d",&dz->infile->stype)!=1) {
				sprintf(errstr,"Cannot read stype sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_ORIGSTYPE+4):	
			if(sscanf(argv[cnt],"%d",&dz->infile->origstype)!=1) {
				sprintf(errstr,"Cannot read origstype sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_ORIGRATE+4):	
			if(sscanf(argv[cnt],"%d",&dz->infile->origrate)!=1) {
				sprintf(errstr,"Cannot read origrate sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_MLEN+4):	
			if(sscanf(argv[cnt],"%d",&dz->infile->Mlen)!=1) {
				sprintf(errstr,"Cannot read Mlen sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_DFAC+4):	
			if(sscanf(argv[cnt],"%d",&dz->infile->Dfac)!=1) {
				sprintf(errstr,"Cannot read Dfac sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_ORIGCHANS+4):	
			if(sscanf(argv[cnt],"%d",&dz->infile->origchans)!=1) {
				sprintf(errstr,"Cannot read origchans sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_SPECENVCNT+4):	
			if(sscanf(argv[cnt],"%d",&dz->infile->specenvcnt)!=1) {
				sprintf(errstr,"Cannot read specenvcnt sent from TK\n");
				return(DATA_ERROR);
			}
			dz->specenvcnt = dz->infile->specenvcnt;
			break;
		case(INPUT_WANTED+4):	
			if(sscanf(argv[cnt],"%d",&dz->wanted)!=1) {
				sprintf(errstr,"Cannot read wanted sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_WLENGTH+4):	
			if(sscanf(argv[cnt],"%d",&dz->wlength)!=1) {
				sprintf(errstr,"Cannot read wlength sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_OUT_CHANS+4):	
			if(sscanf(argv[cnt],"%d",&dz->out_chans)!=1) {
				sprintf(errstr,"Cannot read out_chans sent from TK\n");
				return(DATA_ERROR);
			}
			break;
			/* RWD these chanegs to samps - tk will have to deal with that! */
		case(INPUT_DESCRIPTOR_BYTES+4):	
			if(sscanf(argv[cnt],"%d",&dz->descriptor_samps)!=1) {
				sprintf(errstr,"Cannot read descriptor_samps sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_IS_TRANSPOS+4):	
			if(sscanf(argv[cnt],"%d",&dz->is_transpos)!=1) {
				sprintf(errstr,"Cannot read is_transpos sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_COULD_BE_TRANSPOS+4):	
			if(sscanf(argv[cnt],"%d",&dz->could_be_transpos)!=1) {
				sprintf(errstr,"Cannot read could_be_transpos sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_COULD_BE_PITCH+4):	
			if(sscanf(argv[cnt],"%d",&dz->could_be_pitch)!=1) {
				sprintf(errstr,"Cannot read could_be_pitch sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_DIFFERENT_SRATES+4):	
			if(sscanf(argv[cnt],"%d",&dz->different_srates)!=1) {
				sprintf(errstr,"Cannot read different_srates sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_DUPLICATE_SNDS+4):	
			if(sscanf(argv[cnt],"%d",&dz->duplicate_snds)!=1) {
				sprintf(errstr,"Cannot read duplicate_snds sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_BRKSIZE+4):	
			if(sscanf(argv[cnt],"%d",&inbrksize)!=1) {
				sprintf(errstr,"Cannot read brksize sent from TK\n");
				return(DATA_ERROR);
			}
			if(inbrksize > 0) {
				switch(dz->input_data_type) {
				case(WORDLIST_ONLY):
					break;
				case(PITCH_AND_PITCH):
				case(PITCH_AND_TRANSPOS):
				case(TRANSPOS_AND_TRANSPOS):
					dz->tempsize = inbrksize;
					break;
				case(BRKFILES_ONLY):
				case(UNRANGED_BRKFILE_ONLY):
				case(DB_BRKFILES_ONLY):
				case(ALL_FILES):
				case(ANY_NUMBER_OF_ANY_FILES):
					if(dz->extrabrkno < 0) {
						sprintf(errstr,"Storage location number for brktable not established by CDP.\n");
						return(DATA_ERROR);
					}
					if(dz->brksize == NULL) {
						sprintf(errstr,"CDP has not established storage space for input brktable.\n");
						return(PROGRAM_ERROR);
					}
					dz->brksize[dz->extrabrkno]	= inbrksize;
					break;
				default:
					sprintf(errstr,"TK sent brktablesize > 0 for input_data_type [%d] not using brktables.\n",
					dz->input_data_type);
					return(PROGRAM_ERROR);
				}
				break;
			}
			break;
		case(INPUT_NUMSIZE+4):	
			if(sscanf(argv[cnt],"%d",&dz->numsize)!=1) {
				sprintf(errstr,"Cannot read numsize sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_LINECNT+4):	
			if(sscanf(argv[cnt],"%d",&dz->linecnt)!=1) {
				sprintf(errstr,"Cannot read linecnt sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_ALL_WORDS+4):	
			if(sscanf(argv[cnt],"%d",&dz->all_words)!=1) {
				sprintf(errstr,"Cannot read all_words sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_ARATE+4):	
			if(sscanf(argv[cnt],"%f",&dz->infile->arate)!=1) {
				sprintf(errstr,"Cannot read arate sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_FRAMETIME+4):	
			if(sscanf(argv[cnt],"%lf",&dummy)!=1) {
				sprintf(errstr,"Cannot read frametime sent from TK\n");
				return(DATA_ERROR);
			}
			dz->frametime = (float)dummy;
			break;
		case(INPUT_WINDOW_SIZE+4):	
			if(sscanf(argv[cnt],"%f",&dz->infile->window_size)!=1) {
				sprintf(errstr,"Cannot read window_size sent from TK\n");
					return(DATA_ERROR);
			}
			break;
		case(INPUT_NYQUIST+4):	
			if(sscanf(argv[cnt],"%lf",&dz->nyquist)!=1) {
				sprintf(errstr,"Cannot read nyquist sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_DURATION+4):	
			if(sscanf(argv[cnt],"%lf",&dz->duration)!=1) {
				sprintf(errstr,"Cannot read duration sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_MINBRK+4):	
			if(sscanf(argv[cnt],"%lf",&dz->minbrk)!=1) {
				sprintf(errstr,"Cannot read minbrk sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_MAXBRK+4):	
			if(sscanf(argv[cnt],"%lf",&dz->maxbrk)!=1) {
				sprintf(errstr,"Cannot read maxbrk sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_MINNUM+4):	
			if(sscanf(argv[cnt],"%lf",&dz->minnum)!=1) {
				sprintf(errstr,"Cannot read minnum sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		case(INPUT_MAXNUM+4):	
			if(sscanf(argv[cnt],"%lf",&dz->maxnum)!=1) {
				sprintf(errstr,"Cannot read maxnum sent from TK\n");
				return(DATA_ERROR);
			}
			break;
		default:
			sprintf(errstr,"case switch item missing: parse_sloom_data()\n");
			return(PROGRAM_ERROR);
		}
		cnt++;
	}
	if(cnt!=PRE_CMDLINE_DATACNT+1) {
		sprintf(errstr,"Insufficient pre-cmdline params sent from TK\n");
		return(DATA_ERROR);
	}

	if(true_cnt)
		cnt = true_cnt;
	*cmdlinecnt = 0;		

	while(cnt < argc) {
		if((exit_status = get_tk_cmdline_word(cmdlinecnt,cmdline,argv[cnt]))<0)
			return(exit_status);
		cnt++;
	}
	return(FINISHED);
}

/********************************* GET_TK_CMDLINE_WORD *********************************/

int get_tk_cmdline_word(int *cmdlinecnt,char ***cmdline,char *q)
{
	if(*cmdlinecnt==0) {
		if((*cmdline = (char **)malloc(sizeof(char *)))==NULL)	{
			sprintf(errstr,"INSUFFICIENT MEMORY for TK cmdline array.\n");
			return(MEMORY_ERROR);
		}
	} else {
		if((*cmdline = (char **)realloc(*cmdline,((*cmdlinecnt)+1) * sizeof(char *)))==NULL)	{
			sprintf(errstr,"INSUFFICIENT MEMORY for TK cmdline array.\n");
			return(MEMORY_ERROR);
		}
	}
	if(((*cmdline)[*cmdlinecnt] = (char *)malloc((strlen(q) + 1) * sizeof(char)))==NULL)	{
		sprintf(errstr,"INSUFFICIENT MEMORY for TK cmdline item %d.\n",(*cmdlinecnt)+1);
		return(MEMORY_ERROR);
	}
	strcpy((*cmdline)[*cmdlinecnt],q);
	(*cmdlinecnt)++;
	return(FINISHED);
}


/****************************** ASSIGN_FILE_DATA_STORAGE *********************************/

int assign_file_data_storage(int infilecnt,dataptr dz)
{
	int exit_status;
	int no_sndfile_system_files = FALSE;
	dz->infilecnt = infilecnt;
	if((exit_status = allocate_filespace(dz))<0)
		return(exit_status);
	if(no_sndfile_system_files)
		dz->infilecnt = 0;
	return(FINISHED);
}

/*********************** CHECK_THE_PARAM_VALIDITY_AND_CONSISTENCY *********************/

int check_the_param_validity_and_consistency(dataptr dz)
{
	int n, cnt, exit_status;
	double lo,hi,lostep,histep,semitonespan,maxval;
	switch(dz->process) {
	case(SPEC_REMOVE):
		dz->param[0] = miditohz(dz->param[0]);
		dz->param[1] = miditohz(dz->param[1]);
		lo = min(dz->param[0],dz->param[1]);
		if((dz->brksize[3] == 0) && flteq(dz->param[3],0.0)) {
			sprintf(errstr,"Attenuation is zero: spectrum will not be changed.\n");
			return(DATA_ERROR);
		}
		cnt = 0;
		hi = lo;
		while(hi <= dz->param[2]) {
			cnt++;
			hi += lo;
		}
		if(cnt == 0) {
			sprintf(errstr,"Upper search limit (%lf) means there are no pitches to search for.\n",dz->param[2]);
			return(MEMORY_ERROR);
		}
		hi = max(dz->param[0],dz->param[1]);
		dz->zeroset = 0;
		if(lo * 2.0 <= hi) {
			fprintf(stdout,"WARNING: Pitchrange of octave or more.\n");
			fprintf(stdout,"WARNING: Will eliminate entire spectrum between lower pitch and %lf Hz\n",dz->param[2]);
			fflush(stdout);
			dz->zeroset = 1;
			dz->param[0] = lo;
		} else {
			if((dz->parray[0] = (double *)malloc(cnt * sizeof(double)))==NULL) {
				sprintf(errstr,"No memory to store 1st set of harmonics.\n");
				return(MEMORY_ERROR);
			}
			if((dz->parray[1] = (double *)malloc(cnt * sizeof(double)))==NULL) {
				sprintf(errstr,"No memory to store 2nd set of harmonics.\n");
				return(MEMORY_ERROR);
			}
			lostep = lo;
			histep = hi;
			for(n = 0;n<cnt;n++) {
				dz->parray[0][n] = lo;
				dz->parray[1][n] = hi;
				lo += lostep;
				hi += histep;
			}
			dz->itemcnt = cnt;
		}
		break;
	case(SPECLEAN):
	case(SPECTRACT):
		dz->iparam[0] = (int)cdp_round((dz->param[0] * MS_TO_SECS)/dz->frametime);
		dz->iparam[0] = max(dz->iparam[0],1);
		n = dz->insams[0]/dz->wanted;
		if(n < dz->iparam[0] + 1) {
			sprintf(errstr,"File is too short for the 'persist' parameter used.\n");
			return(GOAL_FAILED);
		}
		break;
	case(SPECSLICE):
		if((exit_status = get_maxvalue(1,&maxval,dz)) < 0)
			return(exit_status);
		if(dz->mode == 3) {
			dz->param[0] = (int)(floor(dz->nyquist/maxval));
			break;
		}
		switch(dz->mode) {
		case(0):
			if(dz->param[0] * maxval >= dz->clength) {
				sprintf(errstr,"Insufficient channels to accomodate your parameters.\n");
				return(GOAL_FAILED);
			}
			break;
		case(1):
			if(dz->param[0] * maxval >= dz->nyquist) {
				sprintf(errstr,"Insufficient bandwidth to accomodate your parameters.\n");
				return(GOAL_FAILED);
			}
			break;
		case(2):
			semitonespan = LOG2(dz->nyquist/dz->chwidth) * SEMITONES_PER_OCTAVE;
			if(dz->param[0] * maxval >= semitonespan) {
				sprintf(errstr,"Insufficient bandwidth to accomodate your parameters.\n");
				return(GOAL_FAILED);
			}
			break;
		}
		break;
	case(SPECTOVF):
		if(dz->param[SVF_HILM] <= dz->param[SVF_LOLM]) {
			sprintf(errstr,"Impossible pitch range (%lf to %lf) specified.\n",dz->param[SVF_LOLM],dz->param[SVF_HILM]);
			return(USER_ERROR);
		}
	   	dz->param[SVF_RANGE] = pow(SEMITONE_INTERVAL,fabs(dz->param[SVF_RANGE])); 
		break;
	case(SPECTOVF2):
		if(dz->param[SVF_HILM] <= dz->param[SVF_LOLM]) {
			sprintf(errstr,"Impossible pitch range (%lf to %lf) specified.\n",dz->param[SVF_LOLM],dz->param[SVF_HILM]);
			return(USER_ERROR);
		}
		dz->param[SVF2_WNDR]  = pow(SEMITONE_INTERVAL,fabs(dz->param[SVF2_WNDR]));
		dz->param[SVF2_PSST]  *= MS_TO_SECS;
		dz->iparam[SVF2_PSST] = (int)round(dz->param[SVF2_PSST]/dz->frametime);		// Persist, as no of analysis windows
		dz->param[SVF2_SGNF]  = pow(SEMITONE_INTERVAL,fabs(dz->param[SVF2_SGNF]));
		dz->param[SVF2_TSTP]  *= MS_TO_SECS;
		dz->iparam[SVF2_TSTP] = (int)round(dz->param[SVF2_TSTP] /dz->frametime); 	// output time resolutio , as no of analysis windows
		dz->param[SVF2_WSIZ]  = pow(SEMITONE_INTERVAL,fabs(dz->param[SVF2_WSIZ]));
		break;
	}
	return FINISHED;
}

/************************* redundant functions: to ensure libs compile OK *******************/

int assign_process_logic(dataptr dz)
{
	return(FINISHED);
}

void set_legal_infile_structure(dataptr dz)
{}

int set_legal_internalparam_structure(int process,int mode,aplptr ap)
{
	return(FINISHED);
}

int setup_internal_arrays_and_array_pointers(dataptr dz)
{
	return(FINISHED);
}

int establish_bufptrs_and_extra_buffers(dataptr dz)
{
	return(FINISHED);
}

int get_process_no(char *prog_identifier_from_cmdline,dataptr dz)
{
	return(FINISHED);
}

int read_special_data(char *str,dataptr dz)	
{
	return(FINISHED);
}

/********************************************************************************************/

int get_the_process_no(char *prog_identifier_from_cmdline,dataptr dz)
{
	if      (!strcmp(prog_identifier_from_cmdline,"remove"))		dz->process = SPEC_REMOVE;
	else if (!strcmp(prog_identifier_from_cmdline,"clean"))			dz->process = SPECLEAN;
	else if (!strcmp(prog_identifier_from_cmdline,"subtract"))		dz->process = SPECTRACT;
	else if (!strcmp(prog_identifier_from_cmdline,"slice"))			dz->process = SPECSLICE;
//	else if (!strcmp(prog_identifier_from_cmdline,"makevfilt"))		dz->process = SPECTOVF;		NOT SATISFACTORY YET: DEC 2009
//	else if (!strcmp(prog_identifier_from_cmdline,"makevfilt2"))	dz->process = SPECTOVF2;	NOT SATISFACTORY YET: DEC 2009
	else {
		sprintf(errstr,"Unknown program identification string '%s'\n",prog_identifier_from_cmdline);
		return(USAGE_ONLY);
	}
	return(FINISHED);
}

/**************************** ALLOCATE_SPECLEAN_BUFFER ******************************/

int allocate_speclean_buffer(dataptr dz)
{
	unsigned int buffersize;
	buffersize = dz->wanted * (dz->bptrcnt + 2);
	dz->buflen = dz->wanted;
	if((dz->bigfbuf	= (float*) malloc(buffersize * sizeof(float)))==NULL) {  
		sprintf(errstr,"INSUFFICIENT MEMORY for sound buffers.\n");
		return(MEMORY_ERROR);
	}
	return(FINISHED);
}

/******************************** USAGE1 ********************************/

int usage1(void)
{
	fprintf(stderr,
	"\nFURTHER OPERATIONS ON ANALYSIS FILES\n\n"
	"USAGE: specnu NAME (mode) infile(s) outfile parameters: \n"
	"\n"
	"where NAME can be any one of\n"
	"\n"
//	"remove    clean    subtract    slice     makevfilt     makevfilt2\n\n";		NOT SATISFACTORY YET: DEC 2009
	"remove    clean    subtract    slice\n\n"
	"Type 'specnu remove' for more info on specnu remove..ETC.\n");
	return(USAGE_ONLY);
}

/******************************** USAGE2 ********************************/

int usage2(char *str)
{
	if(!strcmp(str,"remove")) {
		fprintf(stderr,
	    "USAGE:\n"
		"specnu remove 1-2 inanalfile outanalfile midimin midimax rangetop atten\n"
		"\n"
		"MODE 1: removes pitch and its harmonics up to a specified frequency limit\n"
		"MODE 2: removes everything else.\n"
		"\n"
		"MIDIMIN    minimum pitch to remove (MIDI).\n"
		"MIDIMAX    maximum pitch to remove (MIDI).\n"
		"RANGETOP   frequency at which search for harmonics stops (Hz).\n"
		"ATTEN      attenuation of suppressed components (1 = max, 0 = none).\n"
		"\n"
		"Midi range normally should be small.\n"
		"If range is octave or more, whole spectrum between lower pitch & rangetop\n"
		"will be removed.\n"
		"\n");
	} else if(!strcmp(str,"clean")) {
		fprintf(stderr,
	    "USAGE:\n"
		"specnu clean sigfile noisfile outanalfile persist noisgain\n"
		"\n"
		"Eliminate frqbands in 'sigfile' falling below maxlevels found in 'noisfile'\n"
		"\n"
		"PERSIST    min-time chan-signal > noise-level, to be retained (Range 0-1000 ms).\n"
		"NOISGAIN   multiplies noiselevels in noisfile channels before they are used\n"
		"           for comparison with infile signal: (Range 1 - %.0lf).\n"
		"\n"
		"Both input files are analysis files.\n"
		"'noisfil' is a sample of noise (only) from the 'sigfile'.\n"
		"\n",CL_MAX_GAIN);
	} else if(!strcmp(str,"subtract")) {
		fprintf(stderr,
	    "USAGE:\n"
		"specnu subtract sigfile noisfile outanalfile persist noisgain\n"
		"\n"
		"Eliminate frqbands in 'sigfile' falling below maxlevels found in 'noisfile'\n"
		"and subtract amplitude of noise in noisfile channels from signal that is passed.\n"
		"\n"
		"PERSIST    min-time chan-signal > noise-level, to be retained (Range 0-1000 ms).\n"
		"NOISGAIN   multiplies noiselevels in noisfile channels before they are used\n"
		"           for comparison with infile signal: (Range 1 - %.0lf).\n"
		"\n"
		"Both input files are analysis files.\n"
		"'noisfil' is a sample of noise (only) from the 'sigfile'.\n"
		"\n",CL_MAX_GAIN);
	} else if(!strcmp(str,"slice")) {
		fprintf(stderr,
	    "USAGE:\n"
		"specnu slice 1-3 inanalfile outanalfiles bandcnt chanwidth\n"
		"specnu slice 4 inanalfile outanalfiles pitchdata\n"
		"\n"
		"Slices spectrum (frqwise) into mutually exclusive parts\n"
		"MODE 1: (moiree slice) -- chanwidth in analysis channels\n"
		"puts alternate (groups of) channels into different slices: e.g.\n"
		"bandcnt 3 chanwidth 1 -> outfil1: 0 3 6... outfil2: 1 4 7... outfil3: 2 5 8 ... \n"
		"bandcnt 2 chanwidth 2 -> outfil1: 0-1 4-5 9-8... outfil2: 2-3 6-7 10-11...\n"
		"(bandcnt must be > 1)\n"
		"MODE 2: (frq band slice) -- chanwidth in Hz\n"
		"puts alternate (groups of) chans into different equal-width frq bands: e.g.\n"
		"(bandcnt can be 1)\n"
		"MODE 3: (pitch band slice) -- chanwidth in semitones\n"
		"puts alternate (groups of) chans into different equal-width pitch bands: e.g.\n"
		"(bandcnt can be 1)\n"
		"MODE 4: (slice by harmonics) -- requires a text datafile of time-frq values\n"
		"(generated by \"repitch getpitch\"). Outputs a file for each harmonic tracked.\n"
		"\n"
		"With outfile \"myname\", produces files \"myname\", \"mynam1\", \"myname2\" etc.\n"
		"\n");

//	} else if(!strcmp(str,"makevfilt")) {		NOT SATISFACTORY YET: DEC 2009
//		fprintf(stderr,
//	    "USAGE:\n"
//		"specnu makevfilt analfile outfiltfile intune harmcnt lofrq hifrq\n"
//		"\n"
//		"Generate varibank filter file based on partials of PITCHED analysis file.\n"
//		"\n"
//		"INTUNE     semitone range within which harmonics judged in tune with fundamental.\n"
//		"HARMCNT    Number of harmonics to find to confirm that the spectrum is pitched.\n"
//		"LOFRQ      Frequency of the lowest pitch to look for.\n"
//		"HIFRQ      Frequency of the highest pitch to look for.\n"
//		"\n");
//	} else if(!strcmp(str,"makevfilt2")) {
//		fprintf(stderr,
//	    "USAGE:\n"
//		"specnu makevfilt2 analfil outfiltfil hifrq peak cutoff intune persist step wander\n"
//		"         -llofrq -wsubwinsize -a -n\n"
//		"\n"
//		"Generate varibank filter file based on persisting PEAKS in an analysis file.\n"
//		"Should locate peaks in inharmonic sounds, or multipitched sources.\n"
//		"\n"
//		"HIFRQ      Frequency of the highest peak to look for.\n"
//		"PEAK       How much louder then local average or median must peak be, to retain.\n"
//		"CUTOFF     Level (relative to window max) below which peaks ignored (range 0 - 1).\n"
//		"INTUNE     semitone range within which peak judged to be in tune with some peak\n"
//		"           in the previous window.\n"
//		"PERSIST    Duration (mS) for which peak must persist, to be retained.\n"
//		"STEP       Minimum timestep (mS) between data outputs to filter file.\n"
//		"WANDER     Minimum semitone shift that (at least one) output peak must take\n"
//		"           before new set of filter values is output.\n"
//		"LOFRQ      Frequency of the lowest peak to look for.\n"
//		"SUBWINSIZE Size of subwindow (semitones) used to search for peaks\n"
//		"           inside the analysis window (default, octave).\n"
//		"-a         Look for peaks above the average level in search subwindow.\n"
//		"           Default: Look for peaks above median level in search subwindow.\n"
//		"-n         Normalise level in each filter output line.\n"
//		"\n");
	} else
		fprintf(stdout,"Unknown option '%s'\n",str);
	return(USAGE_ONLY);
}

int usage3(char *str1,char *str2)
{
	fprintf(stderr,"Insufficient parameters on command line.\n");
	return(USAGE_ONLY);
}

/*********************** INNER_LOOP ***************************/

int inner_loop
(int *peakscore,int *descnt,int *in_start_portion,int *least,int *pitchcnt,int windows_in_buf,dataptr dz)
{
	int exit_status;
//	int local_zero_set = FALSE;
	int wc;
   	for(wc=0; wc<windows_in_buf; wc++) {
		if(dz->total_windows==0) {
			dz->flbufptr[0] += dz->wanted;
			dz->total_windows++;
			dz->time = (float)(dz->time + dz->frametime);
			continue;
		}
		if((exit_status = read_values_from_all_existing_brktables((double)dz->time,dz))<0)
			return(exit_status);
		if((exit_status = spec_remove(dz))<0)
			return(exit_status);
		dz->flbufptr[0] += dz->wanted;
		dz->total_windows++;
		dz->time = (float)(dz->time + dz->frametime);
	}
	return FINISHED;
}

/*********************** SPEC_REMOVE ***************************/

int spec_remove(dataptr dz)
{
	int vc, done, cnt;
	double *lo, *hi, gain = 1.0 - dz->param[3], lofrq;
	if(dz->zeroset) {
		if(dz->mode == 0) {
			for( vc = 0; vc < dz->wanted; vc += 2) {
				if((dz->flbufptr[0][FREQ] >= dz->param[0]) &&  (dz->flbufptr[0][FREQ] <= dz->param[2]))
					dz->flbufptr[0][AMPP] = (float)(dz->flbufptr[0][AMPP] * gain);
			}
		} else {
			for( vc = 0; vc < dz->wanted; vc += 2) {
				if((dz->flbufptr[0][FREQ] <= dz->param[0]) ||  (dz->flbufptr[0][FREQ] >= dz->param[2]))
					dz->flbufptr[0][AMPP] = (float)(dz->flbufptr[0][AMPP] * gain);
			}
		}
	} else {
		cnt = 0;
		done = 0;
		lo = dz->parray[0];
		hi = dz->parray[1];
		if(dz->mode == 0) {
			for( vc = 0; vc < dz->wanted; vc += 2) {
				if((dz->flbufptr[0][FREQ] >= lo[cnt]) &&  (dz->flbufptr[0][FREQ] <= hi[cnt]))
					dz->flbufptr[0][AMPP] = (float)(dz->flbufptr[0][AMPP] * gain);
				else {
					while(dz->flbufptr[0][FREQ] >= hi[cnt]) {
						cnt++;
						if(cnt >= dz->itemcnt) {
							done = 1;
							break;
						}
					}
					if(done)
						break;
				}
			}
		} else {
			for( vc = 0; vc < dz->wanted; vc += 2) {
				if(dz->flbufptr[0][FREQ] < lo[cnt])
					dz->flbufptr[0][AMPP] = (float)(dz->flbufptr[0][AMPP] * gain);
				else {
					if(cnt+1 >= dz->itemcnt)
						lofrq = dz->nyquist;
					else
						lofrq = dz->nyquist = lo[cnt+1];
					if(dz->flbufptr[0][FREQ] < lofrq && dz->flbufptr[0][FREQ] > hi[cnt]) {
						dz->flbufptr[0][AMPP] = (float)(dz->flbufptr[0][AMPP] * gain);
					} else {
						cnt++;
						if(cnt >= dz->itemcnt) {
							done = 1;
							break;
						}
					}
				}
				if(done)
					break;
			}
		}
	}
	return(FINISHED);
}

/**************************** DO_SPECLEAN ****************************/

int do_speclean(int subtract,dataptr dz)
{
	int exit_status, cc, vc, k;
	int n, samps_read, total_samps = 0;
	float *fbuf, *obuf, *ibuf, *nbuf;
	int *persist, *on, fadelim;
	fadelim = dz->iparam[0] - 1;
	fbuf = dz->bigfbuf;
	obuf = dz->bigfbuf;
	nbuf = dz->bigfbuf + (dz->buflen * dz->iparam[0]);
	ibuf = nbuf - dz->buflen;
	if((persist = (int *)malloc(dz->clength * sizeof(int)))==NULL) {
		sprintf(errstr,"No memory for persistence counters.\n");
		return(MEMORY_ERROR);
	}
	if((on = (int *)malloc(dz->clength * sizeof(int)))==NULL) {
		sprintf(errstr,"No memory for persistence counters.\n");
		return(MEMORY_ERROR);
	}
	memset((char *)persist,0,dz->clength * sizeof(int));
	memset((char *)on,0,dz->clength * sizeof(int));
	memset((char *)nbuf,0,dz->buflen * sizeof(float));
			/* ESTABLISH NOISE CHANNEL-MAXIMA */
	while((samps_read = fgetfbufEx(dz->bigfbuf, dz->buflen,dz->ifd[1],0)) > 0) {
		if(samps_read < 0) {
			sprintf(errstr,"Failed to read data from noise file.\n");
			return(SYSTEM_ERROR);
		}		
		for(cc = 0, vc = 0; cc < dz->clength; cc++, vc +=2) {
			if(fbuf[AMPP] > nbuf[AMPP])
				nbuf[AMPP] = fbuf[AMPP];
		}
		total_samps += samps_read;
	}
	if(total_samps <= 0) {
		sprintf(errstr,"No data found in noise file.\n");
		return(SYSTEM_ERROR);
	}		
			/* MUTIPLY BY 'NOISEGAIN' FACTOR */
	for(cc = 0, vc = 0; cc < dz->clength; cc++, vc +=2)
		nbuf[AMPP] = (float)(nbuf[AMPP] * dz->param[1]);
			/* SKIP FIRST WINDOWS */
	dz->samps_left = dz->insams[0];
	if((samps_read = fgetfbufEx(obuf, dz->buflen,dz->ifd[0],0)) < 0) {
		sprintf(errstr,"Failed to read data from signal file.\n");
		return(SYSTEM_ERROR);
	}
	if(samps_read < dz->buflen) {
		sprintf(errstr,"Problem reading data from signal file.\n");
		return(PROGRAM_ERROR);
	}
	if((exit_status = write_samps(obuf,dz->wanted,dz))<0)
		return(exit_status);
	dz->samps_left -= dz->buflen;
			/* READ IN FIRST GROUP OF SIGNAL WINDOWS */
	dz->samps_left = dz->insams[0] - dz->wanted;
	if((samps_read = fgetfbufEx(dz->bigfbuf, dz->buflen * dz->iparam[0],dz->ifd[0],0)) < 0) {
		sprintf(errstr,"Failed to read data from signal file.\n");
		return(SYSTEM_ERROR);
	}
	if(samps_read < dz->buflen * dz->iparam[0]) {
		sprintf(errstr,"Problem reading data from signal file.\n");
		return(PROGRAM_ERROR);
	}
	dz->samps_left -= dz->buflen * dz->iparam[0];
	for(cc = 0, vc = 0; cc < dz->clength; cc++, vc +=2) {
		/* FOR EACH CHANNEL */
		obuf = dz->bigfbuf;
		fbuf = ibuf;
		/* MOVING BACKWARDS THROUGH FIRST GROUP OF WINDOWS */
		while(fbuf >= obuf) {
		/* COUNT HOW MANY CONTIGUOUS WINDOWS EXCEED THE NOISE THRESHOLD */
			if(fbuf[AMPP] > nbuf[AMPP])
				persist[cc]++;
			else
		/* BUT IF ANY WINDOW IS BELOW NOISE THRESHOLD, BREAK */
				break;
			fbuf -= dz->buflen;
		}
		/* IF NOT ALL WINDOWS ARE ABOVE THE NOISE THRESHOLD, ZERO OUTPUT OF CHANNEL */
		if(persist[cc] < dz->iparam[0])
			obuf[AMPP] = 0.0f;
		else {
			on[cc] = 1;
			if(subtract)
				obuf[AMPP] = (float)max(0.0,obuf[AMPP] - nbuf[AMPP]);
		}
	}
		/* WRITE FIRST OUT WINDOW */
	if((exit_status = write_samps(obuf,dz->wanted,dz))<0)
		return(exit_status);
	obuf = dz->bigfbuf;
	fbuf = obuf + dz->buflen;
			/* MOVE ALL WINDOWS BACK ONE */
	for(n = 1; n < dz->iparam[0]; n++) {
		memcpy((char *)obuf,(char *)fbuf,dz->wanted * sizeof(float));
		fbuf += dz->wanted;
		obuf += dz->wanted;
	}
	obuf = dz->bigfbuf;
	fbuf = obuf + dz->buflen;
	while(dz->samps_left > 0) {
			/* READ A NEW WINDOW TO END OF EXISTING GROUP */
		if((samps_read = fgetfbufEx(ibuf,dz->buflen,dz->ifd[0],0)) < 0) {
			sprintf(errstr,"Problem reading data from signal file.\n");
			return(PROGRAM_ERROR);
		}
		dz->samps_left -= dz->wanted;
		for(cc = 0, vc = 0; cc < dz->clength; cc++, vc +=2) {
			/* IF LEVEL IN ANY CHANNEL IN NEW WINDOW FALLS BELOW NOISE THRESHOLD */
			if(subtract)
				obuf[AMPP] = (float)max(0.0,obuf[AMPP] - nbuf[AMPP]);
			if(ibuf[AMPP] < nbuf[AMPP]) {
			/* IF CHANNEL IS ALREADY ON, KEEP IT, BUT DECREMENT ITS PERSISTENCE VALUE, and FADE OUTPUT LEVEL */
				if(on[cc]) {
							/* If it's persisted longer than min, reset persistence to min */
					if(persist[cc] > dz->iparam[0])	
						persist[cc] = dz->iparam[0];	
							/* as persistence falls from min to zero, fade the output level */
					if(--persist[cc] <= 0) {
						on[cc] = 0;
						obuf[AMPP] = 0.0f;
					} else
						obuf[AMPP] = (float)(obuf[AMPP] * ((double)persist[cc]/(double)dz->iparam[0]));
				} else
			/* ELSE, SET OUTPUT TO ZERO */
					obuf[AMPP] = 0.0f;
			} else {
			/* IF CHANNEL LEVEL IS ABOVE NOISE THRESHOLD, COUNT ITS PERSISTANCE */
				persist[cc]++;
			/* IF ON ALREADY, RETAIN SIGNAL */
				if(on[cc])
					;
			/* ELSE IF PERSISTENCE IS INSUFFICIENT, ZERO OUTPUT */
				else if((k = persist[cc] - dz->iparam[0]) < 0)
					obuf[AMPP] = 0.0f;
				else {
			/* ELSE IF PERSISTANCE IS BELOW FADELIM, FADE IN THAT OUTPUT */
					if(persist[cc] < fadelim)
						obuf[AMPP] = (float)(obuf[AMPP] * ((double)k/(double)dz->iparam[0]));
			/* ELSE RETAIN FULL LEVEL AND MARK CHANNEL AS (FULLY) ON */
					else
						on[cc] = 1;	
				}
			}
		}
			/* WRITE THE OUTPUT */
		if((exit_status = write_samps(obuf,dz->wanted,dz))<0)
			return(exit_status);
			/* MOVE ALL WINDOWS BACK ONE */
		if(dz->samps_left) {
			for(n = 1; n < dz->iparam[0]; n++) {
				memcpy((char *)obuf,(char *)fbuf,dz->wanted * sizeof(float));
				fbuf += dz->wanted;
				obuf += dz->wanted;
			}
		}
		obuf = dz->bigfbuf;
		fbuf = obuf + dz->buflen;
	}
			/* DEAL WITH REMAINDER OF FINAL BLOCK OF WINDOWS */
	obuf += dz->buflen;
			/* ADVANCING THROUGH THE WINDOWS */
	while(obuf < nbuf) {
		for(cc = 0, vc = 0; cc < dz->clength; cc++, vc +=2) {
			if(!on[cc])
				obuf[AMPP] = 0.0f;
			else if(obuf[AMPP] < nbuf[AMPP]) {
				obuf[AMPP] = 0.0f;
				on[cc] = 0;
			} else if(subtract)
				obuf[AMPP] = (float)max(0.0,obuf[AMPP] - nbuf[AMPP]);
		}
		if((exit_status = write_samps(obuf,dz->wanted,dz))<0)
			return(exit_status);
		obuf += dz->buflen;
	}
	return FINISHED;
}

/************************ GET_THE_MODE_NO *********************/

int get_the_mode_no(char *str, dataptr dz)
{
	if(sscanf(str,"%d",&dz->mode)!=1) {
		sprintf(errstr,"Cannot read mode of program.\n");
		return(USAGE_ONLY);
	}
	if(dz->mode <= 0 || dz->mode > dz->maxmode) {
		sprintf(errstr,"Program mode value [%d] is out of range [1 - %d].\n",dz->mode,dz->maxmode);
		return(USAGE_ONLY);
	}
	dz->mode--;		/* CHANGE TO INTERNAL REPRESENTATION OF MODE NO */
	return(FINISHED);
}

/************************ HANDLE_PITCHDATA *********************/

int handle_pitchdata(int *cmdlinecnt,char ***cmdline,dataptr dz)
{
	int istime, valcnt;
	aplptr ap = dz->application;
	int n;
	char temp[200], *q, *filename = (*cmdline)[0];
	double *p, lasttime = 0.0;
	if(!sloom) {
		if(*cmdlinecnt <= 0) {
			sprintf(errstr,"Insufficient parameters on command line.\n");
			return(USAGE_ONLY);
		}
	}
	ap->data_in_file_only 	= TRUE;
	ap->special_range 		= TRUE;
	ap->min_special 		= SPEC_MINFRQ;
	ap->max_special 		= (dz->nyquist * 2.0)/3.0;
	if((dz->fp = fopen(filename,"r"))==NULL) {
		sprintf(errstr,"Cannot open datafile %s\n",filename);
		return(DATA_ERROR);
	}
	n = 0;
	while(fgets(temp,200,dz->fp)!=NULL) {
		q = temp;
		if(is_an_empty_line_or_a_comment(q))
			continue;
		n++;
	}
	if(n==0) {
		sprintf(errstr,"No data in file %s\n",filename);
		return(DATA_ERROR);
	}
	dz->brksize[1] = n;
	if ((dz->brk[1] = (double *)malloc(dz->brksize[1] * 2 * sizeof(double)))==NULL) {
		sprintf(errstr,"Insufficient memory to store pitch data\n");
		return(MEMORY_ERROR);
	}
	p = dz->brk[1];
	if(fseek(dz->fp,0,0)<0) {
		sprintf(errstr,"fseek() failed in handle_pitchdata()\n");
		return(SYSTEM_ERROR);
	}
	n = 0;
	while(fgets(temp,200,dz->fp)!=NULL) {
		q = temp;
		if(is_an_empty_line_or_a_comment(temp))
			continue;
		if(n >= dz->brksize[1]) {
			sprintf(errstr,"Accounting problem reading pitchdata\n");
			return(PROGRAM_ERROR);
		}
		istime = 1;
		valcnt = 0;
		while(get_float_from_within_string(&q,p)) {
			if(valcnt>=2) {
				sprintf(errstr,"Too many values on line %d: file %s\n",n+1,filename);
				return(DATA_ERROR);
			}

			if(istime) {
				if(n == 0) {
					lasttime = *p;
				} else {
					if(*p <= lasttime) {
						sprintf(errstr,"Times fail to advance: line %d: file %s\n",n+1,filename);
						return(DATA_ERROR);
					}
				}
			} else {
				if(*p <= ap->min_special || *p >= ap->max_special) {
					sprintf(errstr,"Pitch out of range (%lf - %lf) : line %d: file %s\n",ap->min_special,ap->max_special,n+1,filename);
					return(DATA_ERROR);
				}
			}
			istime = !istime;
			valcnt++;
			p++;
		}
		if(!istime) {
			sprintf(errstr,"Time-pitch brkpnt data not correctly paired: line %d: file %s\n",n+1,filename);
			return(DATA_ERROR);
		}
		n++;
	}
	dz->brksize[1] = n;
	if(fclose(dz->fp)<0) {
		fprintf(stdout,"WARNING: Failed to close input textfile %s.\n",filename);
		fflush(stdout);
	}
	(*cmdline)++;		
	(*cmdlinecnt)--;
	return(FINISHED);
}

/**************************** DO_SPECSLICE ****************************/

int do_specslice(dataptr dz)
{
	int n, exit_status;
	float *ibuf = dz->bigfbuf, *obuf = dz->bigfbuf;
	double bigstep, thistime, bandbot, bandtop, frqmult;
	int ibigstep, ibandbot, ibandtop, cc, vc;
	int samps_read;
	n = 0;
	while(n < dz->param[0]) {
		dz->total_samps_written = 0;
		if(n>0) {
			if(sndseekEx(dz->ifd[0],0,0)<0) {
				sprintf(errstr,"Seek failed in input analysis file.\n");
				return(SYSTEM_ERROR);
			}
			if((exit_status = create_next_outfile(dz)) < 0) {
				return(exit_status);
			}
		}
		thistime = 0.0;
		while((samps_read = fgetfbufEx(dz->bigfbuf, dz->buflen,dz->ifd[0],0)) > 0) {
			if(samps_read < 0) {
				sprintf(errstr,"Failed to read data from input file.\n");
				return(SYSTEM_ERROR);
			}
			if(dz->mode < 3) {
				if((exit_status = read_values_from_all_existing_brktables(thistime,dz))<0)
					return(exit_status);
			}
			switch(dz->mode) {
			case(0):
				ibigstep = dz->iparam[0] * dz->iparam[1];
				ibandbot = dz->iparam[1] * n;
				ibandtop = ibandbot + dz->iparam[1];
				if(ibandbot >= dz->clength) {
					sprintf(errstr,"Failure in internal channel accounting.\n");
					return(PROGRAM_ERROR);
				}
				cc = 0;
				while(cc < dz->clength) {
					vc = cc * 2;
					while(cc < ibandbot) {
						ibuf[AMPP]  = 0.0f;	/* SETS AMPLITUDE OUTSIDE SELECTED BANDS TO ZERO */
						cc++;
						vc += 2;
					}
					while(cc < ibandtop) {
						cc++;
						vc += 2;
					}
					if((ibandbot += ibigstep) >= dz->clength)
						ibandbot = dz->clength;
					if((ibandtop = ibandbot + dz->iparam[1]) >= dz->clength)
						ibandtop = dz->clength;
				}
				break;
			case(1):
				bigstep = dz->param[1] * (double)dz->iparam[0];
				bandbot = dz->param[1] * (double)n;
				if(bandbot >= dz->nyquist) {
					sprintf(errstr,"Failure in internal channel accounting.\n");
					return(PROGRAM_ERROR);
				}
				if((bandtop = bandbot + dz->param[1]) >= dz->nyquist)
					bandtop = dz->nyquist;
				for(cc = 0, vc = 0; cc < dz->clength; cc++, vc +=2) {
					if(ibuf[FREQ] < bandbot)
						ibuf[AMPP] = 0.0f;
					else if(ibuf[FREQ] >= bandtop) {
						ibuf[AMPP] = 0.0f;
						if((bandbot = bandbot + bigstep) >= dz->nyquist)
							bandtop = dz->nyquist;
						if((bandtop = bandbot + dz->param[1]) >= dz->nyquist)
							bandtop = dz->nyquist;
					}
				}
				break;
			case(2):
				frqmult = 1.0 + pow(2.0,dz->param[1]/SEMITONES_PER_OCTAVE);
				bandbot = dz->chwidth * pow(frqmult,(double)n);
				bigstep = pow(frqmult,(double)dz->iparam[0]);
				if((bandtop = bandbot * frqmult) >= dz->nyquist)
					bandtop = dz->nyquist;
				if(n == 0)
					bandbot = 0.0;
				for(cc = 0, vc = 0; cc < dz->clength; cc++, vc +=2) {
					if(ibuf[FREQ] < bandbot)
						ibuf[AMPP] = 0.0f;
					else if(ibuf[FREQ] >= bandtop) {
						ibuf[AMPP] = 0.0f;
						if((bandbot *= bigstep) >= dz->nyquist)
							bandbot = dz->nyquist;
						if((bandtop = bandbot * frqmult) >= dz->nyquist)
							bandtop = dz->nyquist;
					}
				}
				break;
			case(3):
				if((exit_status = read_value_from_brktable(thistime,1,dz))<0)
					return(exit_status);
				bandbot = (dz->param[1] * (double)n) + (dz->param[1]/2.0);
				if(bandbot >= dz->nyquist) {
					sprintf(errstr,"Failure in internal channel accounting.\n");
					return(PROGRAM_ERROR);
				}
				bandtop = bandbot + dz->param[0];
				if(n == 0)
					bandbot = 0.0;
				for(cc = 0, vc = 0; cc < dz->clength; cc++, vc +=2) {
					if(ibuf[FREQ] < bandbot)
						ibuf[AMPP] = 0.0f;
					else if(ibuf[FREQ] >= bandtop)
						ibuf[AMPP] = 0.0f;
				}
				break;
			}
			thistime += dz->frametime;
			if((exit_status = write_samps(obuf,dz->wanted,dz))<0)
				return(exit_status);
		}
		if((exit_status = headwrite(dz->ofd,dz))<0)
			return(exit_status);
		if((exit_status = reset_peak_finder(dz))<0)
			return(exit_status);
		if (n < dz->param[0] - 1) {
			if(sndcloseEx(dz->ofd) < 0) {
				fprintf(stdout,"WARNING: Can't close output soundfile %s\n",dz->outfilename);
				fflush(stdout);
			}
			dz->ofd = -1;
		}
		n++;
	}
	return FINISHED;
}

/**************************** SETUP_SPECSLICE_PARAMETER_STORAGE ****************************/

int setup_specslice_parameter_storage(dataptr dz) {

	if((dz->brk      = (double **)realloc(dz->brk,2 * sizeof(double *)))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 1\n");
		return(MEMORY_ERROR);
	}
	if((dz->brkptr   = (double **)realloc(dz->brkptr,2 * sizeof(double *)))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 6\n");
		return(MEMORY_ERROR);
	}
	if((dz->brksize  = (int    *)realloc(dz->brksize,2 * sizeof(int)))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 2\n");
		return(MEMORY_ERROR);
	}
	if((dz->firstval = (double  *)realloc(dz->firstval,2 * sizeof(double)))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 3\n");
		return(MEMORY_ERROR);												  
	}
	if((dz->lastind  = (double  *)realloc(dz->lastind,2 * sizeof(double)))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 4\n");
		return(MEMORY_ERROR);
	}
	if((dz->lastval  = (double  *)realloc(dz->lastval,2 * sizeof(double)))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 5\n");
		return(MEMORY_ERROR);
	}
	if((dz->brkinit  = (int     *)realloc(dz->brkinit,2 * sizeof(int)))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 7\n");
		return(MEMORY_ERROR);
	}
	dz->brk[1]     = NULL;
	dz->brkptr[1]  = NULL;
	dz->brkinit[1] = 0;
	dz->brksize[1] = 0;
	if((dz->param       = (double *)realloc(dz->param,2 * sizeof(double)))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 1\n");
		return(MEMORY_ERROR);
	}
	if((dz->iparam      = (int    *)realloc(dz->iparam, 2 * sizeof(int)   ))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 2\n");
		return(MEMORY_ERROR);
	}
	if((dz->is_int      = (char   *)realloc(dz->is_int,2 * sizeof(char)))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 3\n");
		return(MEMORY_ERROR);
	}
	if((dz->no_brk      = (char   *)realloc(dz->no_brk,2 * sizeof(char)))==NULL) {
		sprintf(errstr,"setup_specslice_parameter_storage(): 5\n");
		return(MEMORY_ERROR);
	}
	if((dz->is_active   = (char   *)realloc(dz->is_active,2 * sizeof(char)))==NULL) {
		sprintf(errstr,"setup_parameter_storage_and_constants(): 3\n");
		return(MEMORY_ERROR);
	}
	dz->is_int[1] = 0;
	dz->is_active[1] = 1;
	dz->no_brk[1] = (char)0;
	return FINISHED;
}

/* SPECTOVF */

/***************************** SPECTOVF ***********************/

int spectovf(dataptr dz)
{
	int exit_status, cc, vc;
	int samps_read, wc, windows_in_buf;
	double time = 0.0, maxamp = 0.0, sum;
	if((exit_status = setup_ring(dz))<0)
		return(exit_status);
	while((samps_read = fgetfbufEx(dz->bigfbuf,dz->big_fsize,dz->ifd[0],0)) > 0) {
		dz->flbufptr[0] = dz->bigfbuf;
		windows_in_buf = samps_read/dz->wanted;    
		for(wc=0; wc<windows_in_buf; wc++, dz->total_windows++) {
			if(dz->total_windows==0 && dz->wlength > 1) {
				dz->flbufptr[0] += dz->wanted;
				time += dz->frametime;
				continue;
			}
			sum = 0.0;
			for(cc = 0,vc = 0; cc < dz->clength; cc++, vc += 2)
				sum += dz->flbufptr[0][AMPP];
			maxamp = max(maxamp,sum);
			dz->flbufptr[0] += dz->wanted;
			time += dz->frametime;
		}
	}
	if((sndseekEx(dz->ifd[0],0,0)<0)){        
        sprintf(errstr,"sndseek() failed\n");
        return SYSTEM_ERROR;
    }
	reset_filedata_counters(dz);
	dz->total_windows = 0;
	time = 0.0;
	while((samps_read = fgetfbufEx(dz->bigfbuf,dz->big_fsize,dz->ifd[0],0)) > 0) {
		dz->flbufptr[0] = dz->bigfbuf;
		windows_in_buf = samps_read/dz->wanted;    
		for(wc=0; wc<windows_in_buf; wc++, dz->total_windows++) {
			if(dz->total_windows==0 && dz->wlength > 1) {
				dz->flbufptr[0] += dz->wanted;
				time += dz->frametime;
				continue;
			}
			if((exit_status = specget(time,maxamp,dz))<0)
				return(exit_status);
			dz->flbufptr[0] += dz->wanted;
			time += dz->frametime;
		}
	}
	if(samps_read<0) {
		sprintf(errstr,"Sound read error.\n");
		return(SYSTEM_ERROR);
	}
	return(FINISHED);
}

/****************************** SPECGET *******************************
 *
 * (1)	Ignore partials below low limit of pitch.
 * (2)  If this channel data is louder than any existing piece of data in ring.
 *		(Ring data is ordered loudness-wise)...
 * (3)	If this freq is too close to an existing frequency..
 * (4)	and if it is louder than that existing frequency data..
 * (5)	Substitute in in the ring.
 * (6)	Otherwise, (its a new frq) insert it into the ring.
 */
 
int specget(double time,double maxamp,dataptr dz)
{
	int exit_status;
	int vc;
	chvptr here, there, *partials;
	float minamp;
	double loudest_partial_frq, nextloudest_partial_frq, lo_loud_partial, hi_loud_partial, amp, sum;	
	if((partials = (chvptr *)malloc(MAXIMI * sizeof(chvptr)))==NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY for partials array.\n");
		return(MEMORY_ERROR);
	}
	if((exit_status = initialise_ring_vals(MAXIMI,-1.0,dz))<0)
		return(exit_status);
	if((exit_status = rectify_frqs(dz->flbufptr[0],dz))<0)
		return(exit_status);
	sum = 0.0;
	for(vc=0;vc<dz->wanted;vc+=2) {
		sum += dz->flbufptr[0][AMPP];
		here = dz->ringhead;
		if(dz->flbufptr[0][FREQ] > dz->param[SVF_LOLM]) {  			/* 1 */
			do {								
				if(dz->flbufptr[0][AMPP] > here->val) {			   	/* 2 */
					if((exit_status = close_to_frq_already_in_ring(&there,(double)dz->flbufptr[0][FREQ],dz))<0)
						return(exit_status);
					if(exit_status==TRUE) {
						if(dz->flbufptr[0][AMPP] > there->val) {		/* 4 */
							if((exit_status = substitute_in_ring(vc,here,there,dz))<0) /* 5 */
								return(exit_status);
						}
					} else	{										/* 6 */
						if((exit_status = insert_in_ring(vc,here,dz))<0)
							return(exit_status);
					}				
					break;
				}
			} while((here = here->next)!=dz->ringhead);
		}
	}
	loudest_partial_frq     = dz->flbufptr[0][dz->ringhead->loc + 1];
	nextloudest_partial_frq = dz->flbufptr[0][dz->ringhead->next->loc + 1];
	if(loudest_partial_frq < nextloudest_partial_frq) {
		lo_loud_partial = loudest_partial_frq;
		hi_loud_partial = nextloudest_partial_frq;
	} else {
		lo_loud_partial = nextloudest_partial_frq;
		hi_loud_partial = loudest_partial_frq;
	}
	if((exit_status = put_ring_frqs_in_ascending_order(&partials,&minamp,dz))<0)
		return(exit_status);
	amp = sum/maxamp;
	find_pitch(partials,lo_loud_partial,hi_loud_partial,minamp,time,(float)amp,dz);
	return exit_status;
}

/**************************** CLOSE_TO_FRQ_ALREADY_IN_RING *******************************/

int close_to_frq_already_in_ring(chvptr *there,double frq1,dataptr dz)
{
#define EIGHT_OVER_SEVEN	(1.142857143)

	double frq2, frqratio;
	*there = dz->ringhead;
	do {
		if((*there)->val > 0.0) {
			frq2 = dz->flbufptr[0][(*there)->loc + 1];
			if(frq1 > frq2)
				frqratio = frq1/frq2;
			else
			    frqratio = frq2/frq1;
			if(frqratio < EIGHT_OVER_SEVEN)
				return(TRUE);
		}
	} while((*there = (*there)->next) != dz->ringhead);
	return(FALSE);
}

/******************************* SUBSITUTE_IN_RING **********************/

int substitute_in_ring(int vc,chvptr here,chvptr there,dataptr dz)
{
	chvptr spare, previous;
	if(here!=there) {
		if(there==dz->ringhead) {
			sprintf(errstr,"IMPOSSIBLE! in substitute_in_ring()\n");
			return(PROGRAM_ERROR);
		}
		spare = there;
		there->next->last = there->last; /* SPLICE REDUNDANT STRUCT FROM RING */
		there->last->next = there->next;
		previous = here->last;
		previous->next = spare;			/* SPLICE ITS ADDRESS-SPACE BACK INTO RING */
		spare->last = previous;			/* IMMEDIATELY BEFORE HERE */
		here->last = spare;
		spare->next = here;
		if(here==dz->ringhead)			/* IF HERE IS RINGHEAD, MOVE RINGHEAD */
			dz->ringhead = spare;
		here = spare;					/* POINT TO INSERT LOCATION */
	}
	here->val = dz->flbufptr[0][AMPP]; 	/* IF here==there */
	here->loc = vc;	    				/* THIS WRITES OVER VAL IN EXISTING RING LOCATION */
	return(FINISHED);
}

/*************************** INSERT_IN_RING ***************************/

int insert_in_ring(int vc, chvptr here, dataptr dz)
{
	chvptr previous, newend, spare;
	if(here==dz->ringhead) {
		dz->ringhead = dz->ringhead->last;
		spare = dz->ringhead;
	} else {
		if(here==dz->ringhead->last)
			spare = here;
		else {
			spare  = dz->ringhead->last;
			newend = dz->ringhead->last->last; 	/* cut ENDADR (spare) out of ring */
			dz->ringhead->last = newend;
			newend->next = dz->ringhead;
			previous = here->last;
			here->last = spare;					/* reuse spare address at new loc by */ 
			spare->next = here;  				/* inserting it back into ring before HERE */
			previous->next = spare;
			spare->last = previous;
		}
	}
	spare->val = dz->flbufptr[0][vc];			/* Store new val in spare ring location */
	spare->loc = vc;
	return(FINISHED);
}

/************************** PUT_RING_FRQS_IN_ASCENDING_ORDER **********************/

int put_ring_frqs_in_ascending_order(chvptr **partials,float *minamp,dataptr dz)
{
	int k;
	chvptr start, ggot, here = dz->ringhead;
	float minpitch;
	*minamp = (float)MAXFLOAT;
	for(k=0;k<MAXIMI;k++) {
		if((*minamp = min(dz->flbufptr[0][here->loc],*minamp))>=(float)MAXFLOAT) {
			sprintf(errstr,"Problem with amplitude out of range: put_ring_frqs_in_ascending_order()\n");
			return(PROGRAM_ERROR);
		}
		(here->loc)++;		/* CHANGE RING TO POINT TO FRQS, not AMPS */
		here->val = dz->flbufptr[0][here->loc];
		here = here->next;
	}
	here = dz->ringhead;
	minpitch = dz->flbufptr[0][here->loc];
	for(k=1;k<MAXIMI;k++) {
		start = ggot = here;
		while((here = here->next)!=start) {		/* Find lowest frq */
			if(dz->flbufptr[0][here->loc] < minpitch) {	
				minpitch = dz->flbufptr[0][here->loc];
				ggot = here;
			}
		}
		(*partials)[k-1] = ggot;				/* Save its address */
		here = ggot->next;						/* Move to next ring site */
		minpitch = dz->flbufptr[0][here->loc];	/* Preset minfrq to val there */
		ggot->last->next = here;				/* Unlink ringsite ggot */
		here->last = ggot->last;
	}
	(*partials)[k-1] = here;	     			/* Remaining ringsite is maximum */

	here = dz->ringhead = (*partials)[0];   	/* Reconstruct ring */
	for(k=1;k<MAXIMI;k++) {
		here->next = (*partials)[k];
		(*partials)[k]->last = here;
		here = here->next;
	}
	here->next = dz->ringhead;					/* Close up ring */
	dz->ringhead->last = here;
	return(FINISHED);
}

/******************************	 FIND_PITCH **************************/

#define MAXIMUM_PARTIAL (64)

void find_pitch(chvptr *partials,double lo_loud_partial,double hi_loud_partial,float minamp,double time,float amp,dataptr dz)
{
	static int firsttime = 1;
	int n, m, mm, k, kk, maximi_less_one = MAXIMUM_PARTIAL - 1, endd = 0;
	double whole_number_ratio, comparison_frq, thisfrq, thisamp, pich_pich = -1.0;
	for(n=1;n<maximi_less_one;n++) {
		for(m=n+1;m<MAXIMUM_PARTIAL;m++) {	/* NOV 7 */
			whole_number_ratio = (double)m/(double)n;
			comparison_frq     = lo_loud_partial * whole_number_ratio;
			if(equivalent_pitches(comparison_frq,hi_loud_partial,dz))
				endd = (MAXIMUM_PARTIAL/m) * n;		/* explanation at foot of file */
			else if(comparison_frq > hi_loud_partial)
				break;

			for(k=n;k<=endd;k+=n) {
				pich_pich = lo_loud_partial/(double)k;
				if(pich_pich>dz->param[SVF_HILM])
					continue;
				if(pich_pich<dz->param[SVF_LOLM])
						break;
				if(is_peak_at(pich_pich,0,minamp,dz)){
					if(enough_partials_are_harmonics(partials,pich_pich,dz)) {
						for(mm=0;mm<MAXIMI;mm++) {
							kk = partials[mm]->loc;
							if((thisfrq = dz->flbufptr[0][kk]) < pich_pich)
								continue;
							if(is_a_harmonic(thisfrq,pich_pich,dz)) {		/* for this algo, lowest partial must be within pitchrange specified */
								if(thisfrq > dz->param[SVF_HILM])
									return;
								else
									break;
							}
						}
						for(mm=0;mm<MAXIMI;mm++) {
							kk = partials[mm]->loc;
							if((thisfrq = dz->flbufptr[0][kk]) < pich_pich)
								continue;
							if(is_a_harmonic(thisfrq,pich_pich,dz)) {
								thisamp = dz->flbufptr[0][kk-1];
							}
						}

					}	
				}
			}
		}
	}
	if(pich_pich < dz->param[SVF_LOLM])
		return;
	if(firsttime) {
		fprintf(dz->fp,"0.0 \t%lf\t0.0\n",pich_pich);
		if(!flteq(time,0.0))
			fprintf(dz->fp,"%lf \t%lf\t%lf\n",time,pich_pich,amp);
		firsttime = 0;
		return;
	} 
	fprintf(dz->fp,"%lf \t%lf\t%lf\n",time,pich_pich,amp);
}

/**************************** EQUIVALENT_PITCHES *************************/

int equivalent_pitches(double frq1, double frq2, dataptr dz)
{
	double ratio;
	int   iratio;
	double intvl;

	ratio = frq1/frq2;
	iratio = round(ratio);

	if(iratio!=1)
		return(FALSE);

	if(ratio > iratio)
		intvl = ratio/(double)iratio;
	else
		intvl = (double)iratio/ratio;
	if(intvl > dz->param[SVF_RANGE])	
		return FALSE;
	return TRUE;
}

/*************************** IS_PEAK_AT ***************************/

#define PEAK_LIMIT (.05)

int is_peak_at(double frq,int window_offset,float minamp,dataptr dz)
{
	float *thisbuf;
	int cc, vc, searchtop, searchbot;
	if(window_offset) {								/* BAKTRAK ALONG BIGBUF, IF NESS */
		thisbuf = dz->flbufptr[0] - (window_offset * dz->wanted);
		if((size_t) thisbuf < 0 || thisbuf < dz->bigfbuf || thisbuf >= dz->flbufptr[1])
			return(FALSE);
	} else
		thisbuf = dz->flbufptr[0];
	cc = (int)((frq + dz->halfchwidth)/dz->chwidth);		 /* TRUNCATE */
	searchtop = min(dz->clength,cc + CHANSCAN + 1);
	searchbot = max(0,cc - CHANSCAN);
	for(cc = searchbot ,vc = searchbot*2; cc < searchtop; cc++, vc += 2) {
		if(!equivalent_pitches((double)thisbuf[vc+1],frq,dz)) {
			continue;
		}
		if(thisbuf[vc] < minamp * PEAK_LIMIT)
			continue;
		if(local_peak(cc,frq,thisbuf,dz))		
			return TRUE;
	}
	return FALSE;
}

/**************************** ENOUGH_PARTIALS_ARE_HARMONICS *************************/

int enough_partials_are_harmonics(chvptr *partials,double pich_pich,dataptr dz)
{
	int n, good_match = 0;
	double thisfrq;
	for(n=0;n<MAXIMI;n++) {
		if((thisfrq = dz->flbufptr[0][partials[n]->loc]) < pich_pich)
			continue;
		if(is_a_harmonic(thisfrq,pich_pich,dz)){		
			if(++good_match >= dz->iparam[SVF_MATCH])
				return TRUE;
		}
	}
	return FALSE;
}

/**************************** IS_A_HARMONIC *************************/

int is_a_harmonic(double frq1,double frq2,dataptr dz)
{
	double ratio = frq1/frq2;
	int   iratio = round(ratio);
	double intvl;

	ratio = frq1/frq2;
	iratio = round(ratio);

	if(ratio > iratio)
		intvl = ratio/(double)iratio;
	else
		intvl = (double)iratio/ratio;
	if(intvl > dz->param[SVF_RANGE])
		return(FALSE);
	return(TRUE);
}

/***************************** LOCAL_PEAK **************************/

int local_peak(int thiscc,double frq, float *thisbuf, dataptr dz)
{
	int thisvc = thiscc * 2;
	int cc, vc, searchtop, searchbot;
	double frqtop = frq * SEMITONE_INTERVAL;
	double frqbot = frq / SEMITONE_INTERVAL;
	searchtop = (int)((frqtop + dz->halfchwidth)/dz->chwidth);		/* TRUNCATE */
	searchtop = min(dz->clength,searchtop + PEAKSCAN + 1);
	searchbot = (int)((frqbot + dz->halfchwidth)/dz->chwidth);		/* TRUNCATE */
	searchbot = max(0,searchbot - PEAKSCAN);
	for(cc = searchbot ,vc = searchbot*2; cc < searchtop; cc++, vc += 2) {
		if(thisbuf[thisvc] < thisbuf[vc])
			return(FALSE);
	}
	return(TRUE);
}

/***************************** SPECTOVF2 ***********************/

#define PKTIME	0
#define PKFREQ	1
#define PKCNNT	2
#define PKMARK	3
#define PKCHAN	4

int spectovf2(dataptr dz)
{
	int exit_status;
	int samps_read, wc, windows_in_buf, n, m, outcnt = 0;
	double time = 0.0;
	double *peaking_data, *current_frqs;

	dz->array_cnt=4;
	if((dz->parray  = (double **)malloc(dz->array_cnt * sizeof(double *)))==NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY for internal double arrays.\n");
		return(MEMORY_ERROR);
	}
	if((dz->parray[0] = (double *)malloc((dz->clength * 4) * sizeof(double)))==NULL) {	//	Stores start-time, start-frq, count and marker for each peak-trail
		sprintf(errstr,"INSUFFICIENT MEMORY for peak-trail counters.\n");
		return(MEMORY_ERROR);
	}
	if((dz->parray[1] = (double *)malloc(dz->clength * sizeof(double)))==NULL) {		//	Stores current frqs of all peaks found so far
		sprintf(errstr,"INSUFFICIENT MEMORY for peak frq store.\n");
		return(MEMORY_ERROR);
	}
	if((dz->parray[2] = (double *)malloc(dz->clength * sizeof(double)))==NULL) {		//	Stores values to sort to find median
		sprintf(errstr,"INSUFFICIENT MEMORY for peak frq store.\n");
		return(MEMORY_ERROR);
	}
	if((dz->parray[3] = (double *)malloc((dz->clength * 5) * sizeof(double)))==NULL) {	//	Stores start-time, start-frq, count and marker for each peak-trail, on 2nd pass
		sprintf(errstr,"INSUFFICIENT MEMORY for peak-trail counters.\n");
		return(MEMORY_ERROR);
	}
	peaking_data  = dz->parray[0];
	current_frqs = dz->parray[1];

	dz->itemcnt = 0;	//	Number of events stored
	dz->ringsize = 0;	//	Number of peak-trails

	while((samps_read = fgetfbufEx(dz->bigfbuf,dz->big_fsize,dz->ifd[0],0)) > 0) {
		dz->flbufptr[0] = dz->bigfbuf;
		windows_in_buf = samps_read/dz->wanted;    
		for(wc=0; wc<windows_in_buf; wc++, dz->total_windows++) {
			if(dz->total_windows==0 && dz->wlength > 1) {
				dz->flbufptr[0] += dz->wanted;
				time += dz->frametime;
				continue;
			}
			if((exit_status = locate_peaks(1,time,dz))<0)
				return(exit_status);
			dz->flbufptr[0] += dz->wanted;
			time += dz->frametime;
		}
	}
	dz->fptr_cnt=dz->ringsize;				//	Arrays to store the peak-trail data
	if((dz->fptr  = (float **)malloc(dz->fptr_cnt * sizeof(float *)))==NULL) {
		sprintf(errstr,"INSUFFICIENT MEMORY (1) for float arrays to store peak data.\n");
		return(MEMORY_ERROR);
	}
	for(n=0;n<dz->ringsize;n++) {
		if((dz->fptr[n]  = (float *)malloc((dz->wlength * 2) * sizeof(float)))==NULL) {
			sprintf(errstr,"INSUFFICIENT MEMORY (2for float arrays to store peak data.\n");
			return(MEMORY_ERROR);
		}
	}
	dz->peaktrail_cnt = dz->itemcnt;	//	Final count of peak-trail data info,  keep it
	dz->itemcnt = 0;					//	itemcnt reused in 2nd pass
	dz->ringsize = 0;
	if((sndseekEx(dz->ifd[0],0,0)<0)){        
        sprintf(errstr,"sndseek() failed\n");
        return SYSTEM_ERROR;
    }
	dz->total_windows = 0;
	while((samps_read = fgetfbufEx(dz->bigfbuf,dz->big_fsize,dz->ifd[0],0)) > 0) {
		dz->flbufptr[0] = dz->bigfbuf;
		windows_in_buf = samps_read/dz->wanted;    
		for(wc=0; wc<windows_in_buf; wc++, dz->total_windows++) {
			if(dz->total_windows==0 && dz->wlength > 1) {
				for(n=0,m=0;m<dz->peaktrail_cnt;n++,m+=5) {			//	At window zero, insert the known starting frequencies of the peak trails
					dz->fptr[n][0] = (float)peaking_data[m+PKFREQ];
					dz->fptr[n][1] = 0.0;
				}
				outcnt = 2;
				dz->flbufptr[0] += dz->wanted;
				time += dz->frametime;
				continue;
			}
			if((exit_status = locate_peaks(0,time,dz))<0)
				return(exit_status);
			if((exit_status = store_peaks(&outcnt,time,dz))<0)
				return(exit_status);
			dz->flbufptr[0] += dz->wanted;
			time += dz->frametime;
		}
	}
	sort_peaks(outcnt,dz);
	if((exit_status = write_peaks(outcnt,dz))<0)
		return(exit_status);
	return(FINISHED);
}

/***************************** LOCATE_PEAKS ***********************/

int locate_peaks(int firstpass,double time,dataptr dz)
{	
	int exit_status;
	int startchan, maxchan, topchan, chanstep, halfstep, median_pos, n, m, vc, cnt, done = 0;
	float amp;
	double sum, val, maxamp, threshold, startfrq, nextfrq;
	double *peaking_data;
	if(firstpass)
		peaking_data  = dz->parray[0];	//	Accumulates data about peaks, and retains complete data for reference in 2nd pass
	else
		peaking_data  = dz->parray[3];	//	Accumulates data about peaks and compares with complete data from first pass

	if(dz->itemcnt > 0) {
		m = 0;
		while(m < dz->itemcnt) {
			peaking_data[m+PKMARK] = 0;	// Initially, unmark all existing peaks
			m += 5;
		}
	}
	maxamp = dz->flbufptr[0][0];		//	Find maxamp in analysis window
	for(vc=2;vc<dz->wanted;vc+=2) {
		if (dz->flbufptr[0][AMPP] > maxamp) 
			maxamp = dz->flbufptr[0][AMPP];
	}
	threshold = maxamp * dz->param[SVF2_CTOF];
	
	startfrq  = dz->param[SVF2_LOLM];
	startchan = (int)round(startfrq/dz->chwidth);
	maxchan   = (int)round(dz->param[SVF2_HILM]/dz->chwidth);
	nextfrq   = startchan * dz->param[SVF2_WSIZ];
	while(startchan < maxchan) {
		topchan = (int)round(nextfrq/dz->chwidth);
		if(topchan == startchan)
			topchan++;
		if(topchan >= maxchan) { //	If top of peak-search window is beyond maxfrq to search to, set topchan to maxchan
			topchan = maxchan;
			done = 1;
		}
		chanstep = topchan - startchan;
		if((halfstep = chanstep/2) < 1)
			halfstep++;
		if(dz->vflag[0]) {	//	Averaging method
			sum = 0.0;
			for(vc=startchan * 2, cnt = 0;vc<topchan * 2;vc+=2,cnt++)
				sum += dz->flbufptr[0][AMPP];
			val =  sum/(double)cnt;	//	average

		} else {			//	Median method	
			median_pos = chanstep/2;						 //	IF chanstep == 1, median_pos = 0 in array (Lo) [0] ----------- (Hi)
															 //	IF chanstep == 2, median_pos = 0 in array (Lo) [0][1] -------- (Hi)					
			if((chanstep>1) && (median_pos * 2) == chanstep) //	IF chanstep == 5, median_pos = 2 in array (Lo) [0][1][2][3][4] (Hi)
				median_pos--;								 //	IF chanstep == 4, median_pos = 1 in array (Lo) [0][1][2][3] -- (Hi)
			cnt = 0;			
			for(vc=startchan * 2, cnt = 0;vc<topchan * 2;vc+=2,cnt++) {
				amp = dz->flbufptr[0][AMPP];
				if(cnt == 0)
					dz->parray[2][0] = amp;
				else {
					for(n=0;n<cnt;n++) {
						if(amp < dz->parray[2][n]) {
							for(m=cnt;m>n;m--) {						// if amp lower than some value in list
								dz->parray[2][m] = dz->parray[2][m-1];	// shuffle up
								dz->parray[2][n] = amp;					// insert new val in list
							}
							break;
						}
					}										
					if(n == cnt)					//	if not inserted yet, this is highest amp so far
						dz->parray[2][n] = amp;		//	insert at end of list
				}
			}
			val = dz->parray[2][median_pos];		//	median
		}
		val *= dz->param[SVF2_PKNG];				//	The peaks must be a certain ratio above the median-or-average
		for(vc=startchan * 2;vc<topchan * 2;vc+=2) {
			amp = dz->flbufptr[0][AMPP];
			if(amp >= val && amp > threshold) {		//	 Keep peak if passes median-average test, AND it's above the theshold
				if((exit_status = keep_peak(firstpass,vc,time,dz))<0)
					return(exit_status);
			}
		}
		if(done)
			break;
		startchan += halfstep;						//	Advance by half a peak-search window
		startfrq = startchan * dz->chwidth;
		nextfrq = startfrq * dz->param[SVF2_WSIZ];
	}
	if((exit_status = remove_non_persisting_peaks(firstpass,dz))<0)
		return(exit_status);
	return FINISHED;
}

/***************************** KEEP_PEAK ***********************/

int keep_peak(int firstpass,int vc,double time,dataptr dz)
{
	float thisfrq;
	double *peaking_data, *current_frqs;
	double frq, topfrq, botfrq;
	int len;
	int n, m;
	thisfrq = dz->flbufptr[0][FREQ];
	if(firstpass)
		peaking_data  = dz->parray[0];
	else
		peaking_data  = dz->parray[3];
	current_frqs = dz->parray[1];
	if(dz->itemcnt == 0) {
		peaking_data[PKTIME] = time;
		peaking_data[PKFREQ] = thisfrq;
		peaking_data[PKCNNT] = 1;	//	cnt of this peak in successuve windows
		peaking_data[PKMARK] = 1;	//	This peak is being marked in this window
		peaking_data[PKCHAN] = vc;	//	This is the channel where peak occurs
		dz->itemcnt = 5;
		current_frqs[dz->ringsize++] = thisfrq;	//	Also store this as the CURRENT frq of this peak-trail
	} else {
		m = 0;
		n = 0;
		while(m < dz->itemcnt) {
			frq  = current_frqs[n];
			len  = (int)round(peaking_data[m + PKCNNT]);
			topfrq = frq * dz->param[SVF2_WNDR];
			botfrq = frq / dz->param[SVF2_WNDR];
			if(thisfrq >= botfrq && thisfrq <= topfrq) {	//	Current frq corresponds to an existing peak in a peak-trail
				peaking_data[m + PKCNNT] = (double)(++len);	//	Increment length of peak-trail
				peaking_data[m + PKMARK] = 1;				//	Mark it as present in this window
				peaking_data[m + PKCHAN] = vc;				//	Note which channel it is in
				current_frqs[n] = thisfrq;					//	Store the current frq of this peaktrail
				break;
			}
			m += 5;
			n++;
		}
		if(m == dz->itemcnt) {					// Current peak frq not already in list: new peak-trail
			peaking_data[m + PKTIME] = time;	//	Start-time of trail
			peaking_data[m + PKFREQ] = thisfrq;	//	Current frq of trail
			peaking_data[m + PKCNNT] = 1;		//	length of this peak-trail in windows
			peaking_data[m + PKMARK] = 1;		//	This peak is marked as present in this window
			peaking_data[m + PKCHAN] = vc;		//	Note which channel it is in
			dz->itemcnt += 5;
			current_frqs[n] = thisfrq;			//	Store the current frq of this peaktrail
			dz->ringsize++;
		}
	}
	return(FINISHED);
}

/***************************** REMOVE_NON_PERSISTING_PEAKS ***********************/

int remove_non_persisting_peaks(int firstpass,dataptr dz)
{
	int m, n, mm, nn;
	double *peaking_data;
	double *current_frqs = dz->parray[1];
	if(firstpass)
		peaking_data = dz->parray[0];
	else
		peaking_data = dz->parray[3];
	if(dz->itemcnt == 0)
		return(FINISHED);
	m = 0;
	n = 0;
	while(m < dz->itemcnt) {
		if((peaking_data[m+PKMARK] == 0)						//	IF peak-trail didn't appear in this window AND
		&& (peaking_data[m+PKCNNT] < dz->iparam[SVF2_PSST])) {	//	peak-trail has not persisted for long enough, delete it
			if(n <= dz->ringsize - 1) {							//	where necessary
				nn = n+1;
				mm = m + 5;
				while(nn < dz->ringsize) {						//  by shuffling-down existing value to overwrite them
					current_frqs[n] = current_frqs[nn];
					peaking_data[m + PKTIME] = peaking_data[mm + PKTIME];
					peaking_data[m + PKFREQ] = peaking_data[mm + PKFREQ];
					peaking_data[m + PKCNNT] = peaking_data[mm + PKCNNT];
					peaking_data[m + PKMARK] = peaking_data[mm + PKCHAN];
					peaking_data[m + PKCHAN] = peaking_data[mm + PKMARK];
					nn++;
					mm += 5;
				}
			}
			dz->ringsize--;										//	and decrement counts of peak-trails
			dz->itemcnt -= 5;
		} else {
			m += 5;
			n++;
		}
	}
	return(FINISHED);
}

/***************************** STORE_PEAKS ***********************/

int store_peaks(int *outcnt,double time,dataptr dz)
{
	double *peaking_data  = dz->parray[0];
	double *this_peakdata = dz->parray[3];
	double starttime, startfrq, thisfrq, lastfrq, topfrq, botfrq;
	int out_cnt, m, n, mm, nn;
	int thischan, is_set;
	float thisamp;

	out_cnt = *outcnt;
	for(mm = 0,nn= 0;mm < dz->peaktrail_cnt;mm+=5,nn++) {				//	For every peak-trail
		is_set = 0;		
		starttime = peaking_data[mm+PKTIME];								//	Get the start-time of the peak-trail
		startfrq  = peaking_data[mm+PKFREQ];								//	And the start frequency of the peak-trail
	
		if(time < starttime) {											//	This peak-trail has not yet started
			dz->fptr[nn][out_cnt]   = (float)startfrq;					//	Write starting frq of peak-trail
			dz->fptr[nn][out_cnt+1] = 0.0f;								//	with zero amp
			is_set = 1;
		} else {
			lastfrq = dz->fptr[nn][out_cnt - 2];						//	Find last frequency of this peak-trail
			topfrq = lastfrq * dz->param[SVF2_WNDR];					//	And setup wander limits
			botfrq = lastfrq / dz->param[SVF2_WNDR];
			for(m = 0,n = 0;m < dz->itemcnt;m+=4,n++) {					//	Compare the currently recorded peaks with the known peak-trails
				thisfrq  = this_peakdata[m+PKFREQ];							//	Frq of peak
				thischan = (int)this_peakdata[m+PKCHAN];					//	Channel where it occurs
				thisamp  = dz->flbufptr[0][thischan];						//	Amplitude of peak

				if(flteq(time,starttime) && flteq(thisfrq,startfrq)) {	//	New peak-trail starts here
					dz->fptr[nn][out_cnt]   = (float)startfrq;			//	Write starting frq of peak-trail
					dz->fptr[nn][out_cnt+1] = thisamp;					//	Write amplitude of peak-trail in this window		
					is_set = 1;
				} else {
					if(thisfrq >= botfrq && thisfrq <= topfrq) {		//	If this peak lies within wander limits of this peak-trail, it's in this peak-trail
						dz->fptr[nn][out_cnt]   = (float)thisfrq;		//	Write new frq of peak-trail, at this time
						dz->fptr[nn][out_cnt+1] = thisamp;				//	and amplitude of peak-trail
						is_set = 1;
					}
				}
				if(is_set)
					break;
				m += 5;
				n++;
			}
		}
		if(!is_set) {													//	If there is no trace of this peak-trail in this window
			dz->fptr[nn][out_cnt]   = dz->fptr[nn][out_cnt-2];			//	Keep last frq of trail
			dz->fptr[nn][out_cnt+1] = 0.0f;								//	But give it zero amplitude
		}
	}
	out_cnt += 2;
	*outcnt = out_cnt;
	return(FINISHED);
}

/***************************** SORT_PEAKS ***********************/

void sort_peaks(int outcnt,dataptr dz)
{
	float jfrq, jamp, kfrq, kamp;
	double diffk, diffj, maxamp;
	int n, j, k, m, z, nn;
	int silent;

	//	Eliminate identical peakstreams, or peakstreams that merge

	for(z= 0;z < dz->wlength*2;z+=2) {				//	At every (time/frq pair at every) time
		for(k = 0; k < dz->ringsize - 1; k++) {		//	For every peak-stream	
			kfrq = dz->fptr[k][z];
			for(j = k+1; j < dz->ringsize; j++) {	//	Compare frq at this time, with frq in every other stream at this time
				jfrq = dz->fptr[j][z];
				if(flteq(jfrq,kfrq)) {				//	If they are the same
					if(z == 0) {					//	Which stream is more frq-continuous
						diffk = fabs(dz->fptr[k][z+2] - dz->fptr[k][z]);
						diffj = fabs(dz->fptr[j][z+2] - dz->fptr[j][z]);
					} else {
						diffk = fabs(dz->fptr[k][z] - dz->fptr[k][z-2]);
						diffj = fabs(dz->fptr[j][z] - dz->fptr[j][z-2]);
					}
					if(diffk > diffj) {		//	If lower-in-list peak-stream is least-continuous, swap it with higher-in-list
						for(nn= 0;nn < dz->wlength *2;nn++) {
							kfrq = dz->fptr[k][nn];
							kamp = dz->fptr[k][nn+1];
							dz->fptr[k][nn]   = dz->fptr[j][nn];
							dz->fptr[k][nn+1] = dz->fptr[j][nn+1];
							dz->fptr[j][nn]   = kfrq;
							dz->fptr[j][nn+1] = kamp;
						}
					}								//	Then eliminate the higher-in-list stream
					if(j < dz->ringsize - 1) {		//	If not doing comparison with last-in-list peak-stream
						m = j + 1;					//	Eliminate it by shuffling peakstreams above it downwards
						while(m < dz->ringsize) {	
							for(n= 0;n < dz->wlength*2;n++)		// Doing this at EVERY (frq/amp pair at every) time
								dz->fptr[m-1][n]   = dz->fptr[m][n];
							m++;
						}
					}
					dz->ringsize--;					//	Reduce count of peakstreams
					j--;							//	And stay at 'j' for next comparison, as new val now at j
				}
			}
		}
	}

	//	Eliminate silent peakstreams : shouldn't be necessary, but it is!!

	for(k = 0; k < dz->ringsize; k++) {		//	For every peak-stream	
		silent = 1;
		for(n= 1;n < dz->wlength *2;n+=2) {	//	Look at  amplitude at every time
			if(dz->fptr[k][n] > 0.0) {
				silent = 0;
				break;
			}
		}
		if(silent) {
			if (k < dz->ringsize - 1) {
				m = k + 1;					//	Eliminate the peak-stream by shuffling peakstreams above it downwards
				while(m < dz->ringsize) {	
					for(n= 0;n < dz->wlength*2;n++)		// Doing this at EVERY (frq/amp pair at every) time
						dz->fptr[m-1][n]   = dz->fptr[m][n];
					m++;
				}
			}
			dz->ringsize--;					//	Reduce count of peakstreams
			k--;							//	And stay at 'k' for next test, as new val now at k
		}
	}
	
	// If flag set, normalise levels at each time

	if(dz->vflag[1]) {
		for(n= 0;n < dz->wlength *2;n+=2) {	//	Look at  amplitude at every time
			maxamp = 0.0;
			for(k = 0; k < dz->ringsize; k++)				//	For every peak_stream at this time
				maxamp = max(maxamp,dz->fptr[k][n+1]);		//	Find maxamp among frq-amp entries
			
			if(!flteq(maxamp,0.0)) {
				for(k = 0; k < dz->ringsize; k++)				//	For every peak_stream at this time
					dz->fptr[k][n+1] = (float)(dz->fptr[k][n+1]/maxamp);
			}
		}
	}

	// SORT in to increasing frq order

	for(n= 0;n < dz->wlength *2;n+=2) {				//	For every filter-set
		for(k = 0; k < dz->ringsize - 1; k++) {		//	For every peak-stream	
			kfrq = dz->fptr[k][n];
			kamp = dz->fptr[k][n+1];
			for(j = k+1; j < dz->ringsize; j++) {			
				jfrq = dz->fptr[j][n];
				jamp = dz->fptr[j][n+1];
				if(jfrq < kfrq) {
					dz->fptr[k][n] = jfrq;
					dz->fptr[k][n+1] = jamp;
					dz->fptr[j][n] = kfrq;
					dz->fptr[j][n+1] = kamp;
					kfrq = jfrq;
					kamp = jamp;
				}
			}
		}
	}
}

/***************************** WRITE_PEAKS ***********************/

int write_peaks(int outcnt,dataptr dz)
{
	int write_cnt = 0;
	int dowrite = 1;							//	Force write of first data-set
	double time = 0.0, timestep, maxamp, topfrq, botfrq;
	double *thisfrq = dz->parray[0];			//	reuse this array
	double *thisamp = dz->parray[1];			//	reuse this array
	double *last_written_frq = dz->parray[2];	//	reuse this array
	int datastep = dz->iparam[SVF2_TSTP] * 2;	//	Every window represented by amp+frq vals
	int n, lastn, j, k, z, lastz, maxloc;

	timestep = dz->frametime * (double)dz->iparam[SVF2_TSTP];	//	We only write data (at most) every TSTP windows
	lastn = 0;
	lastz = 1;
	for(n = datastep; n <outcnt; n += datastep) {				//	For every group of windows within the data-reduction timestep
		for(k = 0; k < dz->ringsize; k++) {			
			maxloc = lastn;										//	For every peaktrail
			maxamp = dz->fptr[k][lastz];						//	Find the max amplitude event within data-reduction window
			for(j = lastn + 2,z = j+1; j < n; j+=2,z+=2) {
				if(dz->fptr[k][z] > maxamp) {
					maxloc = j;
					maxamp = dz->fptr[k][z];
				}
			}
			thisamp[k] = maxamp;
			thisfrq[k] = dz->fptr[k][maxloc];					//	Get frq associated with max amplitude event
		}
		if(write_cnt > 0) {										//	For all datasets after 1st, try to do data-reduction
			dowrite = 0;
			for(k = 0; k < dz->ringsize; k++) {							//	For every peak_trail
				topfrq = last_written_frq[k] * dz->param[SVF2_SGNF];	//	setup wander limits from the last frq WRITTEN
				botfrq = last_written_frq[k] / dz->param[SVF2_SGNF];
				if(thisfrq[k] < botfrq || thisfrq[k] > topfrq) {		//	write new data, if ANY current peak-trail frq is OUTSIDE wander limits
					dowrite = 1;
					break;
				}
			}
		}
		if(dowrite) {
			fprintf(dz->fp,"%f",time);							//	Write the time
			for(k = 0; k < dz->ringsize; k++) {					//	Followed by frq and amplitude for every peak-trail at this time
				fprintf(dz->fp,"  %lf  %lf",thisfrq[k],thisamp[k]);
				last_written_frq[k] = thisfrq[k];				//	Remember the last weitten frq of each peak-trail
			}
			fprintf(dz->fp,"\n");
		}
		lastn = n;
		lastz = lastn + 1;
		time += timestep;										//	Advance time by data-reduction timestep (whether or not data is written)	
	}
	
	if(lastn < outcnt - 1) {									//	Deal with any partial data-block at end
		n = outcnt;
		for(k = 0; k < dz->ringsize; k++) {			
			maxloc = lastn;
			maxamp = dz->fptr[k][lastz];
			for(j = lastn + 2,z = j+1; j < n; j+=2,z+=2) {
				if(dz->fptr[k][z] > maxamp) {
					maxloc = j;
					maxamp = dz->fptr[k][z];
				}
			}
			thisamp[k] = maxamp;
			thisfrq[k] = dz->fptr[k][maxloc];
		}
		if(write_cnt > 0) {
			dowrite = 0;
			for(k = 0; k < dz->ringsize; k++) {
				topfrq = last_written_frq[k] * dz->param[SVF2_SGNF];
				botfrq = last_written_frq[k] / dz->param[SVF2_SGNF];
				if(thisfrq[k] < botfrq || thisfrq[k] > topfrq) {
					dowrite = 1;
					break;
				}
			}
		}
		if(dowrite) {
			fprintf(dz->fp,"%f",time);
			for(k = 0; k < dz->ringsize; k++) {
				fprintf(dz->fp,"  %lf  %lf",thisfrq[k],thisamp[k]);
				last_written_frq[k] = thisfrq[k];
			}
			fprintf(dz->fp,"\n");
		}
	}
	return (FINISHED);
}
