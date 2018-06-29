/******************************************************************************
*******************************************************************************


                    MM    MM    AAAA    IIIIIIII  NN    NN
                    MMM  MMM   AAAAAA   IIIIIIII  NNN   NN
                    MMMMMMMM  AA    AA     II     NNNN  NN
                    MM MM MM  AAAAAAAA     II     NN NN NN
                    MM    MM  AA    AA     II     NN  NNNN
                    MM    MM  AA    AA     II     NN  NNNN
                    MM    MM  AA    AA  IIIIIIII  NN   NNN
                    MM    MM  AA    AA  IIIIIIII  NN    NN


*******************************************************************************
*******************************************************************************

                              A Compiler for VSL
                              ==================

   These are the main, initialisation, tidy up and utility routines.

   Modifications:
   ==============

   22 Nov 88 JPB:  First version
   26 Apr 89 JPB:  Version for publication
   13 Jun 90 JPB:  mklabel fixed to return t
    9 May 91 JPB:  mktmp fixed so as not to clobber yylval (J Johnson and K
                   Schweller)
   23 Jan 96 JPB:  Various minor corrections for 2nd edition publication.

*******************************************************************************
******************************************************************************/

/* As well as including the general header we include the header "parser.h"
   generated by YACC, which contains definitions of all the terminals. Note
   that this is the file "y.tab.h" obtained by running yacc with the -d option.
   We rename it "parser.h" when building the compiler for clarity. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vc.h"
#include "parser.h"

/* Constants used here. CONST_MAX gives the number of small integers
   pre-intialised in the symbol table (described later). LAB_MIN is the first
   label number to be used. Label L0 is reserved for the end of code and data
   and L1 for the termination routine. Labels up to LAB_MIN - 1 may be used by
   the library routines. */

#define CONST_MAX  5
#define LAB_MIN   10

/* This is the global variable that will be set on recognition of the sentence
   symbol, <program>, by the parser. We can't make use of "yyval", since that
   is not exported by the parser if you use Bison rather than YACC. */

extern TAC *program_tac ;

/* We define a number of static variables used throughout the compiler. These
   have been declared external in the header file, and are defined here. */

SYMB  *symbtab[HASHSIZE] ;               /* Symbol table */
SYMB  *local_symbtab[HASHSIZE] ;
TAC   *library[LIB_MAX] ;                /* Entries for library routines */
int    next_tmp ;                        /* Count of temporaries */
int    next_label ;                      /* Count of labels */

/* These are static variables used throughout this section. "const_tab" is the
   table of predeclared integers. "errors_found" is a flag set if the error
   routine is ever called. We do not bother to generate code if errors are
   found during parsing.

   Symbol table and expression nodes are regularly allocated, freed and
   reallocated. Rather than use the system routines "malloc()" and "free()"
   directly for this, we maintain our own freelists, held in "symb_list" and
   "enode_list". */

SYMB  *const_tab[CONST_MAX] ;            /* Small constants */
int    errors_found ;                    /* True if we have any errors */
SYMB  *symb_list ;                       /* Freelists */
ENODE *enode_list ;

/* Prototypes of routines defined in this section. */

int    main( int   argc,
             char *argv[] ) ;
void   init_vc( int   argc,
                char *argv[] ) ;
char  *decode_args( int   argc,
                    char *argv[] ) ;
void   setup_files( char *ifile,
                    char *ofile ) ;
SYMB  *mkconst( int  n ) ;
SYMB  *mklabel( int  l ) ;
SYMB  *mktmp( void ) ;
SYMB  *get_symb( void ) ;
void   free_symb( SYMB *s ) ;
ENODE *get_enode( void ) ;
void   free_enode( ENODE *e ) ;
void  *safe_malloc( int  n ) ;
TAC   *mktac( int   op,
              SYMB *a,
              SYMB *b,
              SYMB *c ) ;
TAC   *join_tac( TAC *c1,
                 TAC *c2 ) ;
void insert(SYMB *s, SYMB **symbtab);
int    hash( char *s ) ;
SYMB *lookup(char *s, SYMB **symbtab);
void   print_instr( TAC *i ) ;
char  *ts( SYMB *s,
           char *str ) ;
void   error( char *str ) ;


int main( int   argc,
            char *argv[] )

/* The main program initialises the compiler, calls the syntax analyser and if
   this runs successfuly passes the resulting TAC on for code generation.

   The program takes a single command line argument, a filename ending in
   ".vsl", which is the program to be compiled. */

{
        init_vc( argc, argv ) ;          /* Set up things */

        (void) yyparse() ;               /* Parse */

        if( !errors_found )
                cg( program_tac ) ;      /* Generate code from TAC */
				return 0;
}       /* void  main( void ) */


void  init_vc( int   argc,
               char *argv[] )

/* The first part of initialisation involves sorting out the arguments, and
   assigning standard input and standard output files as explained in
   decode_args and setup_files below.

   The rest involves setting various system wide variables to sensible
   values and clearing down the symbol table. Small constants are so common we
   put them in the symbol table first and record their addresses in a table. We
   do this for the integers 0 to COUNT_MAX - 1. This will permit us efficient
   access to them throughout the compiler.

   We record the entry labels for the library routines. We happen to know from
   inspection of the library code that the entry point to PRINTN is L2 and to
   PRINTS is L4. If we rewrite the library then we may have to change these.
   This is really rather clumsy, and at the very least ought to be put in a
   single table somewhere. Note that earlier we set the first label to be used
   to L10, permitting the use of labels up to L9 for library use. */

{
        int  i ;                             /* General counter */

        char *ifile ;                        /* Input file */
        char *ofile ;                        /* Output file */


        /* Decode the arguments, and setup stdin and stdout accordingly */

        ofile = decode_args( argc, argv ) ;
        ifile = argv[1] ;                       /* Do after decode has checked
                                                   argument is OK */
        setup_files( ifile, ofile ) ;

        /* Give values to global variables */

        symb_list   = NULL ;                 /* Freelists */
        enode_list  = NULL ;

        errors_found = FALSE ;               /* No errors found yet */
        next_tmp     = 0 ;                   /* No temporaries used */
        next_label   = LAB_MIN ;             /* 10 labels reserved */

        for( i = 0 ; i < HASHSIZE ; i++ )    /* Clear symbol table */
                symbtab[i] = NULL ;

        for( i = 0 ; i < CONST_MAX ; i++ )   /* Make constants */
        {
                SYMB *c = get_symb() ;       /* Node for the constant */

                c->type      = T_INT ;
                c->VAL1      = i ;
                const_tab[i] = c ;
        }

        library[LIB_PRINTN] = mktac( TAC_LABEL, mklabel( 2 ), NULL, NULL ) ;
        library[LIB_PRINTS] = mktac( TAC_LABEL, mklabel( 4 ), NULL, NULL ) ;

}       /* void  init_vc( void ) */


char  *decode_args( int   argc,
                    char *argv[] )

/* The general usage of vc is:

      vc  <file>.vsl

   The output is a file of assembler code, ending in ".vas". We check there is
   a single argument, ending in ".vsl" and construct a result ending in ".vas".
*/

{
        char *ofile ;                   /* Constructed output file */
        int   rootlen ;                 /* Length of input filename's root */

        /* First check there is a single argument */

        if( argc != 2 )
        {
                error( "single argument expected\n" ) ;
                exit( 0 ) ;
        }

        /* Find suffix, which must be ".vsl", check that it is there. */

        rootlen = strlen( argv[1] ) - strlen( ".vsl" ) ;

        if( (rootlen < 1) || (strcmp( argv[1] + rootlen, ".vsl" ) != 0) )
        {
                fprintf( stderr, "source file must end in \".vsl\"" ) ;
                exit( 0 ) ;
        }

        /* We now allocate the space for the name of the output file. Remember
           to allow a byte for the end of string marker. */

        ofile = (char *)safe_malloc( rootlen + strlen( ".vas" ) + 1 ) ;

        /* Construct the new output file by adding vas to the end of the
           existing root. Return the name of this file. */

        strncpy( ofile, argv[1], rootlen ) ;    /* Root */
        strcat( ofile, ".vas" ) ;               /* Extension */

        return  ofile ;

}       /* decode_args() */


void  setup_files( char *ifile,
                   char *ofile )

/* All our I/O is done using the standard input and standard output. We
   substitute the given files using the freopen function. */

{
        if( freopen( ifile, "r", stdin ) == NULL )
        {
                error( "setup_files: freopen of input failed" ) ;
                exit( 0 ) ;
        }

        if( freopen( ofile, "w", stdout ) == NULL )
        {
                error( "setup_files: freopen of output failed" ) ;
                exit( 0 ) ;
        }

}       /* setup_files() */


/* We now have a number of routines to set up symbol table nodes of various
   types. Memory allocation usually involves up to three routines, "mkxxx()" to
   allocate and set up the fields of a struct of type "xxx", "get_xxx()" to
   allocate space for the struct and "free_xxx()" to free up the space. */


SYMB *mkconst( int  n )

/* In "mkconst()" we check if the constant is one of the predefined ones, and
   if so use it, otherwise we create a new entry for it. Note that this wastes
   space, since we should check if we have used any constant before and return
   a pointer to an existing node if possible. However the technique of just
   predefining the first few constants (which make up the majority used) is
   a good compromise that is efficient. */

{
        if((n >= 0) && (n < CONST_MAX))
                return const_tab[n] ;
        else
        {
                SYMB *c = get_symb() ;   /* Create a new node */

                c->type = T_INT ;
                c->VAL1 = n ;
                return c ;
        }

}       /* SYMB *mkconst( int  n ) */


SYMB *mklabel( int  l )

/* Make a label node with the given value */

{
        SYMB *t = get_symb() ;

        t->type = T_LABEL ;
        t->VAL1 = l ;

        return t ;

}       /* SYMB *mklabel( int  l ) */


SYMB *mktmp( void )

/* Make a temporary name. This is just a var with name of the form Txxx. We
   construct a temporary name into the text buffer, "name" and
   then use the lexical analyser's name allocator "mkname()".

   Bug fix due to Jonathan Johnson 9/5/91:

   Note that yylval may not yet have been shifted, and so we must save and
   restore it after this routine. The use of a fixed size for the string where
   we build the variable would be better as a named constant. */

{
        SYMB *old_yylval = yylval.symb ;         /* Save the old yylval */
        SYMB *tmp ;                              /* Used to restore result */
        char  name[12] ;                         /* For the name of the var */

        /* Make the name with mkname */

        sprintf( name, "T%d", next_tmp++ ) ;   /* Set up text */
        mkname( name ) ;
        yylval.symb->type = T_VAR ;

        /* Hang onto this new symbol in tmp, restore yylval.symb and return
           tmp */

        tmp         = yylval.symb ;
        yylval.symb = old_yylval ;

        return tmp ;

}       /* SYMB *mktmp( void ) */


SYMB *get_symb( void )

/* Allocate space for a symbol table entry. Note the use of the freelist
   "symb_list" to hold any nodes that have been returned. If none is available
   we use "malloc()" to obtain a new node. Rather than use "malloc()" direct we
   call our own version "safe_malloc()". This guarantees to return a valid
   pointer (and never NULL). If store has run out and safe_malloc() cannot
   allocate a new structure it will print an error message and exit the
   compiler. */

{
        SYMB *t ;

        if( symb_list != NULL )
        {
                t         = symb_list ;
                symb_list = symb_list->next ;
        }
        else
                t = (SYMB *)safe_malloc( sizeof( SYMB )) ;

        return t ;

}       /* SYMB *get_symb( void ) */


void  free_symb( SYMB *s )

/* This is the sister routine to "get_symb()" and just adds the symbol node to
   the freelist for reuse. */

{
        s->next   = symb_list ;
        symb_list = s ;

}       /* void  free_symb( SYMB *s ) */


ENODE *get_enode( void )

/* Allocate for ENODE. This routine and "free_enode()" are analagous to
   "get_symb()" and "free_symb()". */

{
        if( enode_list != NULL )
        {
                ENODE *expr ;

                expr       = enode_list ;
                enode_list = expr->next ;

                return expr ;
        }
        else
                return (ENODE *)safe_malloc( sizeof( ENODE )) ;

}       /* ENODE *get_enode( void ) */


void  free_enode( ENODE *expr )

/* Return an enode for reuse */

{
        expr->next = enode_list ;
        enode_list = expr ;

}       /* void  free_enode( ENODE *expr ) */


void *safe_malloc( int  n )

/* Rather than have a test for a null pointer each time we call "malloc()" in
   the compiler we write our own safe version, "safe_malloc()".  If memory runs
   out there is no more we can do, and so the routine aborts the entire
   compilation. */

{
        void *t = (void *)malloc( n ) ;

        /* Check we got it */

        if( t == NULL )
        {
                /* We can't use printf to put the message out here, since it
                   calls malloc, which will fail because that's why we're
                   here... */

                error( "malloc() failed" ) ;
                exit( 0 ) ;
        }

        return t ;

}       /* void *safe_malloc( int  n ) */


/* A couple of routine for allocating and joining TAC lists. We need not
   maintain a freelist for TAC, since we never free a TAC quadruple once
   allocated. */


TAC *mktac( int   op,                    /* Operator */
            SYMB *a,                     /* Result */
            SYMB *b,                     /* Operands */
            SYMB *c )

/* Construct a TAC quadruple with the given fields

        a := b op c

   Note the use of #defined selectors VA, VB and VC for the TAC struct. If
   efficiency became a worry we might chose not to call "safe_malloc()" each
   time we wanted a new qaudruple, but to allocate several at once. */

{
        TAC *t = (TAC *)safe_malloc( sizeof( TAC )) ;

        t->next  = NULL ;                /* Set these for safety */
        t->prev  = NULL ;
        t->op    = op ;
        t->VA   = a ;
        t->VB = b ;
        t->VC = c ;

        return t ;

}       /* TAC *mktac( int   op,
                       SYMB *a,
                       SYMB *b,
                       SYMB *c ) */


TAC *join_tac( TAC *c1,
               TAC *c2 )

/* Join two pieces of TAC together.  Remember that in the parser we always
   refer to a TAC list by the most recently generated piece of code and so we
   follow the prev pointer to get the preceding instructions. We will end up
   with a pointer to a TAC list for the parser representing the code of "c1"
   followed by that of "c2". */

{
        TAC *t ;

        /* If either list is NULL return the other */

        if( c1 == NULL )
                return c2 ;

        if( c2 == NULL )
                return c1 ;

        /* Run down c2, until we get to the beginning and then add c1 */

        t = c2 ;

        while( t->prev != NULL )
                t = t->prev ;

        t->prev = c1 ;
        return c2 ;

}       /* TAC *join_tac( TAC *c1,
                          TAC *c2 ) */


/* These are the symbol table routines. We have a routine, "insert()", to
   insert a node in the symbol table (created by one of the "mkxxx()" routines)
   and a routine, "lookup()" to find the symbol table entry, if any, for a
   given text name. Both routines use the hashing function "hash()" described
   in chapter 5. */


void insert(SYMB *s, SYMB **symbtab)

/* Insert a new symbol in the symbol table. We hash on a text first argument */

{
        int hv = hash( s->TEXT1 ) ;

        s->next = symbtab[hv] ;          /* Insert at head */
        symbtab[hv]  = s ;

}       /* void  insert( SYMB *s ) */


int  hash( char *s )

/* Return a hashvalue from the given text. We use the bottom nybble of each
   character ORed with the top nybble of the hashvalue so far and shifted in at
   the bottom. This is then reduced mod the size of the hash table. Note the
   implicit assumption that we are on a 32 bit machine. */

{
        int  hv = 0 ;
        int  i ;

        for( i = 0 ; s[i] != EOS ; i++ )
        {
                int  v = (hv >> 28) ^ (s[i] & 0xf) ;

                hv = (hv << 4) | v ;
        }

        hv = hv & 0x7fffffff ;           /* Ensure positive */
        return hv % HASHSIZE ;

}       /* int  hash ( char *s ) */


SYMB *lookup(char *s, SYMB **symbtab)

/* Lookup a name in the hashtable. Return NULL if the name is not found. */

{
        int   hv = hash( s ) ;
        SYMB *t  = symbtab[hv] ;

        while( t != NULL )                        /* Look for the name */
                if( strcmp( t->TEXT1, s ) == 0 )
                        break ;
                else
                        t = t->next ;

        return t ;                       /* NULL if not found */

}       /* SYMB lookup( char *s ) */


/* We now have a couple of routines for debugging purposes. */


void  print_instr( TAC *i )

/* "print_instr()" is used to print out a TAC instruction symbolically. We use
   it in the code generator to print each TAC instruction as a comment before
   the code generated for it. The subsidiary routine, "ts()" is used to obtain
   a suitable string representation of the TAC arguments. Note the clumsy
   programming assumption that arguments can be represented in 11 characters.
*/


{
        char sa[12] ;                    /* For text of TAC args */
        char sb[12] ;
        char sc[12] ;

        printf( "       ", i ) ;

        switch( i->op )
        {
        case TAC_UNDEF:

                printf( "undef\n" ) ;
                break ;

        case TAC_ADD:

                printf( "%s := %s + %s\n", ts( i->VA, sa ), ts( i->VB, sb ),
                        ts( i->VC, sc )) ;
                break ;

        case TAC_SUB:

                printf( "%s := %s - %s\n", ts( i->VA, sa ), ts( i->VB, sb ),
                        ts( i->VC, sc )) ;
                break ;

        case TAC_MUL:

                printf( "%s := %s * %s\n", ts( i->VA, sa ), ts( i->VB, sb ),
                        ts( i->VC, sc )) ;
                break ;

        case TAC_DIV:

                printf( "%s := %s / %s\n", ts( i->VA, sa ), ts( i->VB, sb ),
                        ts( i->VC, sc )) ;
                break ;

        case TAC_NEG:

                printf( "%s := - %s\n", ts( i->VA, sa ), ts( i->VB, sb )) ;
                break ;

        case TAC_COPY:

                printf( "%s := %s\n", ts( i->VA, sa ), ts( i->VB, sb )) ;
                break ;

        case TAC_GOTO:

                printf( "goto L%d\n", i->LA->VA->VAL1 ) ;
                break ;

        case TAC_IFZ:

                printf( "ifz %s goto L%d\n", ts( i->VB, sb ),
                        i->LA->VA->VAL1 ) ;
                break ;

        case TAC_IFNZ:

                printf( "ifnz %s goto L%d\n", ts( i->VB, sb ),
                        i->LA->VA->VAL1 ) ;
                break ;

        case TAC_ARG:

                printf( "arg %s\n", ts( i->VA, sa )) ;
                break ;

        case TAC_CALL:

                if( i->VA == NULL )
                        printf( "call L%d\n", i->LB->VA->VAL1 ) ;
                else
                        printf( "%s = call L%d\n", ts( i->VA, sa ),
                                i->LB->VA->VAL1 ) ;
                break ;

        case TAC_RETURN:

                printf( "return %s\n", ts( i->VA, sa )) ;
                break ;

        case TAC_LABEL:

                printf( "\nL%d:\n", i->VA->VAL1 ) ;
                break ;

        case TAC_VAR:

                printf( "var %s\n", ts( i->VA, sa )) ;
                break ;

        case TAC_BEGINFUNC:

                printf( "beginfunc\n" ) ;
                break ;

        case TAC_ENDFUNC:

                printf( "endfunc\n" ) ;
                break ;

        default:

                /* Don't know what this one is */

                error( "unknown TAC opcode" ) ;
                printf( "unknown %d\n", i->op ) ;
                break ;
        }

        fflush( stdout ) ;

}       /* print_instr( i ) */


char *ts( SYMB *s,                       /* Symbol to translate */
          char *str )                    /* String to put it in */

/* Return the string representation of the given symbol. Permissible ones are
   functions, vars, temporaries or constants */

{

        /* Check we haven't been given NULL */

        if( s == NULL )
                return "NULL" ;

        /* Identify the type */

        switch( s->type )
        {
        case T_FUNC:
        case T_VAR:

                /* Just return the name */

                return s->TEXT1 ;

        case T_TEXT:

                /* Put the address of the text */

                sprintf( str, "L%d", s->VAL2 ) ;
                return str ;

        case T_INT:

                /* Convert the number to string */

                sprintf( str, "%d", s->VAL1 ) ;
                return str ;

        default:

                /* Unknown arg type */

                fprintf(stderr, "unknown TAC arg type %d\n", s->type) ;
                return "?" ;
        }

}       /* ts( SYMB *s,
               char *str ) */


void  error( char *str )

/* This is a very simple error message routine. This is just prints a message
   to the standard error stream, and sets a flag to indicate that an error has
   occurred. If this is set at the end of parsing, then we do not carry on
   further with code generation. */

{
        fprintf( stderr, "vc: %s\n", str ) ;
        errors_found = TRUE ;

}       /* void  error( char *str ) */