/*** main.c ********************************************************************
** 
** This file is part of BibTool.
** It is distributed under the GNU General Public License.
** See the file COPYING for details.
** 
** (c) 1996-2016 Gerd Neugebauer
** 
** Net: gene@gerd-neugebauer.de
** 
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2, or (at your option)
** any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
** >>> See the file doc/c_lib.tex for a printed documentation of the
** >>> main parts of the C source.
** 
**-----------------------------------------------------------------------------
** Description:
**	This is the \BibTool{} main module. It contains the |main()|
**	function which evaluates the command line arguments and
**	proceeds accordingly. This means that usually resource files
**	and \BibTeX{} files are read and one or more \BibTeX{} files
**	are written.
**
**	This file makes use of the \BibTool{} C library but is not
**	part of it. For this purpose it has to provide certain
**	functions which are expected by the library. These functions
**	are: 
**	\begin{quote}
**	  \texttt{save\_input\_file()}\\
**	  \texttt{save\_macro\_file()}\\
**	  \texttt{save\_output\_file()}\\
**	\end{quote}
**	The arguments and the expected behavior of these functions is
**	described below.
**
**	If you are trying to understand the implementation of
**	\BibTool{} the file |resource.h| plays a central
**	r\^ole. Consult the description of this file for further
**	details. 
**
**	If you are trying to write your own program to manipulate
**	\BibTeX{} files then this file can serve as a starting point.
**	But you should keep in mind that this file has grown over
**	several years and it contains the full complexity of the
**	\BibTool{} program logic. Thus you can reduce this file
**	drastically if you start playing around with the \BibTool{} C
**	library. 
**
******************************************************************************/

#include <bibtool/general.h>
#include <bibtool/init.h>
#include <bibtool/tex_aux.h>
#include <bibtool/error.h>
#include <bibtool/key.h>
#include <bibtool/pxfile.h>
#include <bibtool/print.h>
#include <bibtool/rewrite.h>
#include <bibtool/rsc.h>
#include <bibtool/entry.h>
#include <bibtool/parse.h>
#include <bibtool/macros.h>
#include <bibtool/s_parse.h>
#include <bibtool/symbols.h>
#include <bibtool/sbuffer.h>
#include <bibtool/expand.h>
#include <bibtool/crossref.h>
#include <bibtool/io.h>
#include <bibtool/version.h>
#ifdef HAVE_LIBKPATHSEA
#ifdef __STDC__
#define HAVE_PROTOTYPES
#endif
#include <kpathsea/debug.h>
#endif
#include "config.h"

/*****************************************************************************/
/* Internal Programs							     */
/*===========================================================================*/

#ifdef __STDC__
#define _ARG(A) A
#else
#define _ARG(A) ()
#endif
 int main _ARG((int argc,char *argv[]));	   /* main.c                 */
 static int dbl_check _ARG((DB db,Record rec));	   /* main.c                 */
 static int do_keys _ARG((DB db,Record rec));	   /* main.c                 */
 static int do_no_keys _ARG((DB db,Record rec));   /* main.c                 */
 static int rec_gt _ARG((Record r1,Record r2));	   /* main.c                 */
 static int rec_gt_cased _ARG((Record r1,Record r2));/* main.c               */
 static int rec_lt _ARG((Record r1,Record r2));	   /* main.c                 */
 static int rec_lt_cased _ARG((Record r1,Record r2));/* main.c               */
 static int update_crossref _ARG((DB db,Record rec));/* main.c               */
 static void usage _ARG((int fullp));		   /* main.c                 */

/*****************************************************************************/
/* External Programs and Variables					     */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/

#ifndef __STDC__
#ifndef HAVE_GETENV

 char * getenv _ARG((char *name));		   /* main.c                 */

/*-----------------------------------------------------------------------------
** Dummy funtion returning NULL to indicate failure.
**___________________________________________________			     */
char * getenv(name)				   /*			     */
  char *name;				   	   /*			     */
{ return (char*)NULL;				   /*			     */
}						   /*------------------------*/
#endif
#endif


#ifndef OptionLeadingCharacter
#define OptionLeadingCharacter '-'
#endif

 static char *use[] =
  { "bibtool [options] [%co outfile] [[%ci] infile] ...\n",
    "\n\tOptions:\n",
    "\t%cA<c>\t\tKind of disambiguating key strings: <c>=0|a|A\n",
    "\t%cc\t\tInclude crossreferenced entries into the output (toggle)\n",
    "\t%cd\t\tCheck double entries (toggle)\n",
    "\t%cf <format>\tKey generation enabled (formated key)\n",
    "\t%cF\t\tKey generation enabled with formated key\n",
    "\t%ch\t\tPrint this help info and exit\n",
    "\t[%ci] infile\tSpecify input file. If %ci omitted it may not start\n",
    "\t\t\twith a %c. If absent stdin is taken to read from.\n",
    "\t\t\tMultiple input files may be given.\n",
    "\t%ck\t\tKey generation enabled.\n",
    "\t%cK\t\tKey generation enabled (long key).\n",
    "\t%cm macfile\tDump macros to macfile. - is stdout\n",
    "\t%cM macfile\tDump used macros to macfile. - is stdout\n",
    "\t%co outfile\tSpecify output file as next argument\n",
    "\t\t\tIf absent stdout is taken to write to.\n",
    "\t%cq\t\tQuiet mode. No warnings.\n",
    "\t%cr resource\tLoad resource file (several are possible).\n",
    "\t%cR\t\tLoad default resource file here.\n",
    "\t%cs\t\tSort.\n",
    "\t%cS\t\tSort reverse.\n",
    "\t%cv\t\tEnable verbose mode.\n",
    "\t%cV\t\tPrint version and exit.\n",
    "\t%cx file\t\tExtract from aux file.\n",
    "\t%cX <regex>\tExtract regular expression.\n",
    "\t%c- <rsc>\tEvaluate one resource command <rsc>.\n",
    "\t%c@\t\tPrint statistics (short).\n",
    "\t%c#\t\tPrint statistics.\n",
#ifdef SYMBOL_DUMP
    "\t%c$\t\tSymbol table output (debugging only)\n",
#endif
    0L,
    "Copyright (C) 2016 Gerd Neugebauer",
    "gene@gerd-neugebauer.de"
  };

/*-----------------------------------------------------------------------------
** Function:	usage()
** Purpose:	Print the version number, a copyright notice, and
**		a short description of the command line options on the
**		|stderr| stream.
**		In addition certain configuration options are
**		printed. If the argument is |FALSE| then only the
**		copyright and the version number are displayed.
** Arguments:
**	fullp	Boolean. If |FALSE| only the version is displayed.
** Returns:	nothing
**___________________________________________________			     */
static void usage(fullp)			   /*			     */
  int           fullp;				   /*                        */
{ static char    *comma = ", ";			   /*                        */
  char		 *sep   = " ";			   /*                        */
  POSSIBLY_UNUSED(comma);			   /*			     */
  POSSIBLY_UNUSED(sep);				   /*			     */
						   /*			     */
  show_version();				   /*                        */
						   /*			     */
  if ( fullp )					   /*                        */
  { register char **cpp;			   /*			     */
    for ( cpp = use; *cpp; cpp++ )		   /*			     */
    { ErrPrintF2(*cpp,				   /*			     */
		 OptionLeadingCharacter,	   /*			     */
		 OptionLeadingCharacter);	   /*			     */
    }						   /*                        */
    ErrPrint("\n");	   			   /*                        */
  }						   /*                        */
						   /*                        */
  ErrPrintF("Library path: %s",		   	   /*                        */
	    (rsc_v_rsc == NULL		   	   /*                        */
	     ? "none"				   /*                        */
	     : (char*)rsc_v_rsc));		   /*                        */
 						   /*                        */
  ErrPrint("\nSpecial configuration options:");	   /*                        */
#define SPECIAL_OPTIONS " none"
#ifdef HAVE_LIBKPATHSEA
  ErrPrintF("%skpathsea",sep);			   /*                        */
  sep = comma;					   /*                        */
#undef SPECIAL_OPTIONS
#define SPECIAL_OPTIONS ""
#endif
#ifndef REGEX
  ErrPrintF("%sNO regex",sep);			   /*                        */
  sep = comma;					   /*                        */
#undef SPECIAL_OPTIONS
#define SPECIAL_OPTIONS ""
#endif
#ifdef EMTEX_LIKE_PATH
  ErrPrintF("%semTeX like path",sep);		   /*                        */
#undef SPECIAL_OPTIONS
#define SPECIAL_OPTIONS ""
#endif
  ErrPrintF("%s\n",SPECIAL_OPTIONS);		   /*                        */
}						   /*------------------------*/

#define UnknownWarning(X)     WARNING2("Unknown flag ignored: ",X)
#define NoSFileWarning	      WARNING("Missing select option. Flag ignored.")
#define NoFileError(X)	      WARNING3("File ",X," not found.")
#define MissingPattern	      WARNING("Missing pattern.")
#define MissingResource	      WARNING("Missing resource.")
#define NoRscError(X)	      WARNING3("Resource file ",X," not found.")


/*****************************************************************************/
/***				    MAIN				   ***/
/*****************************************************************************/


/*-----------------------------------------------------------------------------
** Function:	keep_selected()
** Type:	static int
** Purpose:	Mark the record as deleted if it is not selected.
**		
** Arguments:
**	db	the database
**	rec	the record
** Returns:	FALSE
**___________________________________________________			     */
static int keep_selected(db, rec)		   /*                        */
  DB db;					   /*                        */
  Record rec;					   /*                        */
{						   /*                        */
  if (!is_selected(db, rec))			   /*                        */
  { SetRecordDELETED(rec); }			   /*                        */
 						   /*                        */
  return FALSE;					   /*                        */
}						   /*------------------------*/

#ifdef UNUSED
/*-----------------------------------------------------------------------------
** Function:	keep_xref()
** Type:	int 
** Purpose:	Undelete crossreferenced entries
**		
** Arguments:
**	db	the database
**	rec	the record
** Returns:	FALSE
**___________________________________________________			     */
int keep_xref(db,rec)				   /*                        */
  DB db;					   /*                        */
  Record rec;					   /*                        */
{						   /*                        */
  if ( !RecordIsDELETED(rec) )		   	   /*                        */
  { String key;				   	   /*                        */
    int    count;				   /*                        */
 						   /*                        */
    for (count = rsc_xref_limit;		   /* Prevent infinite loop  */
	 count >= 0;		   	   	   /*                        */
	 count--)				   /*                        */
    {					   	   /*                        */
      if ( (key = get_field(db,rec,sym_crossref)) == NULL )/*                */
      { return FALSE; }			   	   /*                        */
 						   /*                        */
      key = symbol(lower(expand_rhs(key,	   /*                        */
				    sym_empty,     /*                        */
				    sym_empty, 	   /*                        */
				    db)));     	   /*                        */
      if ( (rec=db_search(db,key)) == RecordNULL ) /*                        */
      { ErrPrintF("*** BibTool: Crossref `%s' not found.\n",key);/*          */
        return FALSE;			   	   /*                        */
      }					   	   /*                        */
 						   /*                        */
      if (!RecordIsDELETED(rec)) { return FALSE; } /*                        */
      ClearRecordDELETED(rec);		   	   /*                        */
    }					   	   /*                        */
 						   /*                        */
    ErrPrintF("*** BibTool: Crossref limit exceeded; `%s' possibly looped.\n",
	      key);				   /*                        */
  }    					   	   /*                        */
  return FALSE;					   /*                        */
}						   /*------------------------*/
#endif

/*-----------------------------------------------------------------------------
** Function:	write_macros()
** Type:	static void
** Purpose:	
**		
** Arguments:
**	m_file	the name of the macro file of NULL for none
**	the_db	the database
** Returns:	nothing
**___________________________________________________			     */
static void write_macros(m_file, the_db)	   /*                        */
  char* m_file;		   			   /*                        */
  DB	the_db;					   /*                        */
{ FILE * file;					   /*                        */
 						   /*                        */
  if ( m_file == NULL || *m_file == 0 ) return;    /*			     */
 						   /*                        */
  if ( rsc_verbose )			   	   /*			     */
  { VerbosePrint1("Writing macros"); }	   	   /*			     */
					       	   /*                        */
  if ( strcmp(m_file,"-") == 0 ||		   /*                        */
       (file=fopen(m_file,"w")) == NULL )          /*                        */
  { file = stdout; }			   	   /*                        */
  print_db(file,the_db,rsc_all_macs?"s":"S");      /*                        */
  if ( file != stdout ) { fclose(file); }	   /*                        */
}						   /*------------------------*/

/*-----------------------------------------------------------------------------
** Function:	read_in_files()
** Type:	static void
** Purpose:	
**		
** Arguments:
**	the_db	
** Returns:	nothing
**___________________________________________________			     */
static void read_in_files(the_db)		   /*                        */
  DB the_db;					   /*                        */
{ int n = get_no_inputs();			   /*                        */
  int i;					   /*                        */
  for (i = 0; i < n; i++)		   	   /* For all input files    */
  { if ( read_db(the_db,			   /*                        */
		 (String)get_input_file(i),	   /*                        */
		 rsc_verbose) )	   		   /*                        */
    { NoFileError(get_input_file(i)); }		   /*			     */
  }						   /*			     */
}						   /*------------------------*/

#define Toggle(X) X = !(X)

/*-----------------------------------------------------------------------------
** Function:	main()
** Purpose:	This is the main function which is automatically
**		called when the program is started. This function
**		contains the overall program logic. It has to perform
**		the appropriate initializations, evaluate command line
**		arguments, and run the main loop.
** Arguments:
**	argc	Number of arguments
**	argv	Array of arguments
** Returns:	0 upon success. Usually a failure raises an exception
**		which leads to an |exit()|. Thus this function does
**		not need to signal an error to the calling environment.
**___________________________________________________			     */
int main(argc,argv)				   /*			     */
  int	argc;				   	   /* Argument count	     */
  char	*argv[];			   	   /* Argument values	     */
{ DB	the_db;				   	   /* The \BibTool{} program */
						   /* currently handles a    */
						   /* single database at a   */
						   /* time.  The local       */
						   /* variable |the_db|      */
						   /* contains a reference to*/
						   /* this database.	     */
  int	i;				   	   /*			     */
  FILE	*file;				   	   /*                        */
  int	need_rsc = TRUE;		   	   /*			     */
  int	(*fct)();			   	   /* Function pointer	     */
  int	c_len;					   /*                        */
  int   *c = NULL;				   /*                        */
  char  *o_file;				   /*                        */
 						   /*                        */
  init_error(stderr);				   /*                        */
  init_bibtool(argv[0]);			   /*                        */
						   /*			     */
  for ( i = 1; i < argc; i++ )			   /*			     */
  { char *ap;				   	   /*			     */
    if ( *(ap=argv[i]) != OptionLeadingCharacter ) /*			     */
    { save_input_file(argv[i]);			   /*			     */
    }						   /*			     */
    else					   /*			     */
    { switch ( *++ap )				   /*			     */
      { case 'A': set_base((String)(ap+1));  break;/* disambiguation	     */
	case 'c': Toggle(rsc_xref_select);   break;/* include crossreferences*/
	case 'd': Toggle(rsc_double_check);  break;/* double entries	     */
#ifdef HAVE_LIBKPATHSEA
	case 'D': kpathsea_debug=atoi(++ap); break;/* kpathsea debugging     */
#endif
	case 'f': add_format(argv[++i]);	   /*	!!! no break !!!     */
	case 'F': rsc_make_key = TRUE;	    break; /* key generation	     */
	case 'h': usage(TRUE); return 1;	   /* print help	     */
	case 'i': save_input_file(argv[++i]); break;/*			     */
	case 'K': add_format("long");	    break; /* key generation	     */
	case 'k': add_format("short");	    break; /* key generation	     */
	case 'M':				   /* print macro table	     */
	case 'm': rsc_all_macs = (*ap=='m');	   /* print macro table	     */
	  save_macro_file(argv[++i]);		   /*			     */
	  break;				   /*			     */
	case 'o':				   /* output file	     */
	  if ( ++i < argc )   			   /*		             */
	  { save_output_file(argv[i]); }	   /*                        */
	  else					   /*                        */
	  { WARNING("Missing output file name"); } /*                        */
	  break; 				   /*			     */
	case 'q': Toggle(rsc_quiet);	    break; /* quiet		     */
	case 'r':				   /* resource file	     */
	  if ( ++i < argc && load_rsc((String)(argv[i])) )/*		     */
	  { need_rsc = FALSE; }		   	   /*			     */
	  else					   /*                        */
	  {  NoRscError(argv[i]); }		   /*                        */
	  break;				   /*			     */
	case 'R': need_rsc |= !search_rsc();break; /* default resource file  */
	case 's': Toggle(rsc_sort);	    break; /* sort		     */
	case 'S':				   /*			     */
	  Toggle(rsc_sort);			   /*			     */
	  Toggle(rsc_sort_reverse);		   /*			     */
	  break;				   /* sort		     */
	case 'v': Toggle(rsc_verbose);	    break; /* verbose		     */
	case 'V': usage(FALSE);	         return 1; /* version		     */
	case 'x':				   /* extract		     */
	  rsc_all_macs = FALSE;			   /*                        */
	  if ( ++i < argc )			   /*			     */
	  { read_aux((String)(argv[i]),		   /*                        */
		     save_input_file,		   /*                        */
		     *++ap=='v');  		   /*                        */
	  }					   /*                        */
	  else					   /*                        */
	  { NoSFileWarning; }	   		   /*			     */
	  break;				   /*			     */
	case 'X':				   /* extract pattern	     */
	  rsc_all_macs = FALSE;			   /*                        */
	  if ( ++i < argc ) { save_regex((String)argv[i]); }/*		     */
	  else		    { MissingPattern; }	   /*			     */
	  break;				   /*			     */
	case '#': Toggle(rsc_cnt_all);	    break; /* print full statistics  */
	case '@': Toggle(rsc_cnt_used);	    break; /* print short statistics */
	case '-':				   /* extended command	     */
	  if ( *++ap )	      { (void)use_rsc((String)ap); }/*		     */
	  else if ( ++i<argc ){ (void)use_rsc((String)argv[i]); }/*	     */
	  else		      { MissingResource;  }/*			     */
	  break;				   /*			     */
#ifdef SYMBOL_DUMP
	case '$': Toggle(rsc_dump_symbols); break; /* print symbol table     */
#endif
	default:				   /*			     */
	  UnknownWarning(ap);			   /*			     */
	  usage(TRUE);				   /*                        */
	  return 1;		   		   /*			     */
      }						   /*			     */
    }						   /*			     */
  }						   /*			     */
						   /*			     */
  if ( need_rsc ) { (void)search_rsc(); }	   /*			     */
						   /*			     */
  if ( get_no_inputs() == 0 )			   /* If no input file given */
  { save_input_file("-"); }			   /*  then read from stdin  */
						   /*			     */
  init_read();					   /* Just in case the path  */
 						   /*  has been modified.    */
  the_db = new_db();				   /*                        */
						   /*			     */
  read_in_files(the_db);			   /*                        */
 						   /*                        */
  db_forall(the_db,keep_selected);		   /*                        */
						   /*                        */
  apply_aux(the_db);				   /*                        */
 						   /*                        */
  if ( rsc_xref_select ) db_xref_undelete(the_db); /*                        */
 						   /*                        */
  if ( rsc_cnt_all || rsc_cnt_used )		   /*			     */
  { int i;					   /*                        */
    int * cnt = db_count(the_db,&c_len);	   /*                        */
 						   /*                        */
    if ( (c=(int*)malloc(c_len*sizeof(int))) == NULL )/*                     */
    { rsc_cnt_all = rsc_cnt_used = 0; }		   /*                        */
    else					   /*                        */
    { for (i = 0; i < c_len; i++) c[i] = cnt[i];   /*                        */
    }						   /*                        */
  }						   /*                        */
						   /*			     */
  if ( rsc_expand_crossref )			   /*                        */
  {						   /*			     */
    db_forall(the_db,expand_crossref);		   /*                        */
  }						   /*			     */
						   /*			     */
  if ( rsc_make_key )				   /*                        */
  {						   /*                        */
    DebugPrint1("start keygen");		   /*                        */
    start_key_gen();				   /*                        */
						   /*			     */
    if ( rsc_key_preserve )	   		   /*                        */
    { db_forall(the_db,mark_key); }		   /*                        */
						   /*			     */
    DebugPrint1("rewinding");			   /*                        */
    db_rewind(the_db);				   /*                        */
						   /*                        */
    DebugPrint1("do_keys");			   /*                        */
    db_forall(the_db,do_keys);		   	   /*                        */
						   /*                        */
    DebugPrint1("update crossref");		   /*                        */
    db_forall(the_db,update_crossref);	   	   /*                        */
						   /*                        */
    DebugPrint1("end keygen");			   /*                        */
    end_key_gen();				   /*                        */
  }						   /*                        */
  else						   /*                        */
  {						   /*                        */
    db_forall(the_db,do_no_keys);		   /*                        */
  }						   /*                        */
 						   /*                        */
  if ( rsc_sort )				   /*                        */
  {				   		   /*                        */
    if ( rsc_sort_cased )			   /*                        */
    { if ( rsc_sort_reverse ) fct = rec_lt_cased;  /*                        */
      else		      fct = rec_gt_cased;  /*                        */
    }						   /*                        */
    else					   /*                        */
    { if ( rsc_sort_reverse ) fct = rec_lt;	   /*                        */
      else		      fct = rec_gt;	   /*                        */
    }						   /*                        */
    db_sort(the_db,fct);			   /*                        */
  }						   /*                        */
 						   /*                        */
  if ( rsc_srt_macs )		   	   	   /* Maybe sort macros      */
  { db_mac_sort(the_db); }			   /*                        */
 						   /*                        */
  if ( rsc_double_check	)		   	   /* Maybe look for doubles */
  { db_forall(the_db,dbl_check); }		   /*                        */
 						   /*                        */
  o_file = get_output_file();			   /*                        */
 						   /*                        */
  if ( o_file == NULL ||			   /*                        */
       (file=fopen(o_file,"w")) == NULL )	   /*                        */
  { file = stdout; }				   /*                        */
 						   /*                        */
  if ( rsc_select ) { rsc_del_q = FALSE; }	   /*                        */
 						   /*                        */
  { char * print_spec = (char*)rsc_print_et;	   /*                        */
 						   /*                        */
    if ( rsc_expand_macros )			   /*                        */
    { char * cp;				   /*                        */
      print_spec = new_string(print_spec);	   /*                        */
      for ( cp=print_spec; *cp; cp++ )		   /*                        */
      { if ( *cp == 's' ||			   /*                        */
	     *cp == 'S' ||			   /*                        */
	     *cp == '$' ) *cp = ' ';		   /*                        */
      }						   /*                        */
    }						   /*                        */
    print_db(file,the_db,print_spec);	   	   /*                        */
    if ( rsc_expand_macros) free(print_spec);	   /*                        */
  }						   /*                        */
 						   /*                        */
  if ( file != stdout ) { fclose(file); }	   /*                        */
						   /*			     */
  write_macros(get_macro_file(), the_db);	   /*                        */
						   /*			     */
  if ( rsc_cnt_all || rsc_cnt_used )		   /*			     */
  { int i;					   /*                        */
    int *cnt = db_count(the_db,(int*)NULL);	   /*                        */
						   /*                        */
    ErrC('\n');		   	   	   	   /*			     */
    for (i = 0; i < c_len; ++i)			   /*			     */
    { if ( rsc_cnt_all || c[i] > 0 )	   	   /*			     */
      { ErrPrintF3("---  %-15s %5d read  %5d written\n",/*		     */
		   get_entry_type(i),	   	   /*			     */
		   c[i],		   	   /*			     */
		   cnt[i]);		   	   /*			     */
      }						   /*			     */
    }						   /*			     */
    free(c);					   /*                        */
  }						   /*                        */
						   /*			     */
#ifdef SYMBOL_DUMP
  if ( rsc_dump_symbols ) sym_dump();		   /* Write symbols.	     */
#endif
 						   /*                        */
#ifdef FREE_MEMORY
  free_db(the_db);				   /*                        */
  the_db = NULL;				   /*                        */
  sym_gc();					   /*                        */
#ifdef SYMBOL_DUMP
  sym_dump();					   /*                        */
#endif
#endif
  return 0;					   /*			     */
}						   /*------------------------*/

/*-----------------------------------------------------------------------------
** Function:	rec_gt()
** Purpose:	
**		
**
** Arguments:
**	r1
**	r2
** Returns:	
**___________________________________________________			     */
static int rec_gt(r1, r2)			   /*                        */
  Record r1;					   /*                        */
  Record r2;					   /*                        */
{ return (strcmp((char*)RecordSortkey(r1),	   /*                        */
		 (char*)RecordSortkey(r2)) < 0);   /*                        */
}						   /*------------------------*/

/*-----------------------------------------------------------------------------
** Function:	rec_lt()
** Purpose:	
**		
**
** Arguments:
**	r1
**	r2
** Returns:	
**___________________________________________________			     */
static int rec_lt(r1, r2)			   /*                        */
  Record r1;					   /*                        */
  Record r2;					   /*                        */
{ return (strcmp((char*)RecordSortkey(r1),	   /*                        */
		 (char*)RecordSortkey(r2)) > 0);   /*                        */
}						   /*------------------------*/

/*-----------------------------------------------------------------------------
** Function:	rec_gt_cased()
** Type:	static int
** Purpose:	
**		
** Arguments:
**	r1	
**	r2	
** Returns:	
**___________________________________________________			     */
static int rec_gt_cased(r1, r2)			   /*                        */
  Record r1;					   /*                        */
  Record r2;					   /*                        */
{ return (strcmp((char*)get_key_name(RecordSortkey(r1)),/*                   */
		 (char*)get_key_name(RecordSortkey(r2))) < 0);/*             */
}						   /*------------------------*/

/*-----------------------------------------------------------------------------
** Function:	rec_lt_cased()
** Type:	static int
** Purpose:	
**		
** Arguments:
**	r1	
**	r2	
** Returns:	
**___________________________________________________			     */
static int rec_lt_cased(r1, r2)			   /*                        */
  Record r1;					   /*                        */
  Record r2;					   /*                        */
{ return (strcmp((char*)get_key_name(RecordSortkey(r1)),/*                   */
		 (char*)get_key_name(RecordSortkey(r2))) > 0);/*             */
}						   /*------------------------*/

/*-----------------------------------------------------------------------------
** Function:	do_keys()
** Purpose:	
**		
**
** Arguments:
**	rec
** Returns:	
**___________________________________________________			     */
static int do_keys(db, rec)			   /*                        */
  DB	 db;					   /*                        */
  Record rec;					   /*                        */
{						   /*                        */
  rewrite_record(db,rec);			   /*			     */
  sort_record(rec);				   /*                        */
  make_key(db,rec);				   /*                        */
  if ( rsc_sort || rsc_double_check )		   /*                        */
  { make_sort_key(db,rec); }		   	   /*                        */
  return 0;					   /*                        */
}						   /*------------------------*/

/*-----------------------------------------------------------------------------
** Function:	do_no_keys()
** Purpose:	
**		
**
** Arguments:
**	rec
** Returns:	
**___________________________________________________			     */
static int do_no_keys(db, rec)			   /*                        */
  DB	 db;					   /*                        */
  Record rec;					   /*                        */
{						   /*                        */
  rewrite_record(db,rec);			   /*			     */
  sort_record(rec);				   /*                        */
  mark_key(db,rec);				   /*                        */
  if ( rsc_sort || rsc_double_check )		   /*                        */
  { make_sort_key(db,rec); }		   	   /*                        */
  return 0;					   /*                        */
}						   /*------------------------*/

/*-----------------------------------------------------------------------------
** Function:	update_crossref()
** Purpose:	
**		
**
** Arguments:
**	rec
**	first_rec
** Returns:	nothing
**___________________________________________________			     */
static int update_crossref(db, rec)		   /*			     */
  DB		 db;				   /*                        */
  Record	 rec;				   /*			     */
{ register String *hp;				   /*			     */
  register int   i;				   /*                        */
  String	 s, t;			   	   /*			     */
						   /*			     */
  if ( !RecordIsXREF(rec) ) return 0;		   /*			     */
						   /*                        */
  for (i = RecordFree(rec), hp = RecordHeap(rec);  /* search crossref field  */
       i > 0 && *hp != sym_crossref;		   /*			     */
       i -= 2, hp += 2)				   /*			     */
  { }						   /*			     */
  if ( i <= 0 )					   /*			     */
  { DebugPrint1("*** No crossref found.");	   /*			     */
    return 0;					   /*			     */
  }						   /*			     */
						   /*			     */
  t = *++hp; t++;				   /*			     */
  (void)sp_open(t);				   /* Try to extract	     */
  if ( (s = SParseSymbol(&t)) == StringNULL )	   /*  the crossref as symbol*/
  { return 0; }					   /*			     */
						   /*			     */
  if ( (s = db_new_key(db,s)) == StringNULL )	   /*			     */
  { ERROR2("Crossref not found: ",(char*)s);	   /*			     */
    return 0;					   /*			     */
  }						   /*			     */
  if (rsc_key_case) { s = get_key_name(s); }	   /*                        */
  if ( (t=(String)malloc(strlen((char*)s)+3))==(String)NULL )/* get temp mem */
  { OUT_OF_MEMORY("update_crossref()"); }	   /*		             */
						   /*			     */
  (void)sprintf((char*)t,(**hp=='"'?"\"%s\"":"{%s}"),s);/* make new crossref */
  *hp = symbol(t);				   /* store new crossref     */
  free(t);					   /* free temp memory	     */
  return 0;					   /*                        */
}						   /*------------------------*/

#define equal_records(R1,R2) RecordSortkey(R1) == RecordSortkey(R2)

/*-----------------------------------------------------------------------------
** Function:	dbl_check()
** Purpose:	
**		
**
** Arguments:
**	rec
** Returns:	
**___________________________________________________			     */
static int dbl_check(db, rec)			   /*                        */
  DB	 db;					   /*                        */
  Record rec;					   /*                        */
{						   /*                        */
  if ( PrevRecord(rec) != RecordNULL		   /*                        */
       && equal_records(PrevRecord(rec),rec) )	   /*			     */
  {						   /*                        */
    if ( !rsc_quiet )				   /*                        */
    { String k1 = *RecordHeap(rec);		   /*                        */
      String k2 = *RecordHeap(PrevRecord(rec));	   /*                        */
      ErrPrint("*** BibTool WARNING: Possible double entries discovered: \n***\t");
      if ( k1 == NULL ) k1 = (String) "";	   /*                        */
      if ( k2 == NULL ) k2 = (String) "";	   /*                        */
      ErrPrint((char*)k2);			   /*                        */
      ErrPrint(" =?= ");			   /*                        */
      ErrPrint((char*)k1);			   /*                        */
      ErrPrint("\n***\t");			   /*                        */
      ErrPrint((char*)RecordSortkey(rec));	   /*			     */
      ErrPrint("\n");				   /*                        */
    }						   /*                        */
    if (rsc_del_dbl)				   /*                        */
    { delete_record(db,rec); }		   	   /*                        */
    else 					   /*                        */
    { SetRecordDELETED(rec); }			   /*                        */
  }						   /*			     */
 						   /*                        */
  return 0;					   /*                        */
}						   /*------------------------*/

#ifdef DEBUG
/*-----------------------------------------------------------------------------
** Function*:	printchar()
** Purpose:	For debugging of regex.c
** Arguments:
**	c	Character to print.
** Returns:	nothing
**___________________________________________________			     */
void printchar(c)				   /*                        */
  char c;					   /*                        */
{ ErrC(c);					   /*                        */
}						   /*------------------------*/
#endif
