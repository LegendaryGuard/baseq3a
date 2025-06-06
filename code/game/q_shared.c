// Copyright (C) 1999-2000 Id Software, Inc.
//
// q_shared.c -- stateless support routines that are included in each code dll
#include "q_shared.h"

float Com_Clamp( float min, float max, float value ) {
	if ( value < min ) {
		return min;
	}
	if ( value > max ) {
		return max;
	}
	return value;
}


/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char	*last;
	
	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension( const char *in, char *out, int destsize ) {
	int             length;

	Q_strncpyz(out, in, destsize);

	length = strlen(out)-1;
	while (length > 0 && out[length] != '.')
	{
		length--;
		if (out[length] == '/')
			return;		// no extension
	}
	if ( length )
		out[ length ] = '\0';
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, int maxSize, const char *extension ) {
	char	oldPath[MAX_QPATH];
	char    *src;

//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path) {
		if ( *src == '.' ) {
			return;                 // it has an extension
		}
		src--;
	}

	Q_strncpyz( oldPath, path, sizeof( oldPath ) );
	Com_sprintf( path, maxSize, "%s%s", oldPath, extension );
}


/*
============================================================================

PARSING

============================================================================
*/

static	char	com_token[MAX_TOKEN_CHARS];
static	char	com_parsename[MAX_TOKEN_CHARS];
static	int		com_lines;
static	int		com_tokenline;
static	int		is_separator[ 256 ];

void COM_BeginParseSession( const char *name )
{
	com_lines = 1;
	com_tokenline = 0;
	Com_sprintf(com_parsename, sizeof(com_parsename), "%s", name);
}


int COM_GetCurrentParseLine( void )
{
	if ( com_tokenline )
	{
		return com_tokenline;
	}

	return com_lines;
}

char *COM_Parse( char **data_p )
{
	return COM_ParseExt( data_p, qtrue );
}


extern int Q_vsprintf( char *buffer, const char *fmt, va_list argptr );


void COM_ParseError( char *format, ... )
{
	va_list argptr;
	static char string[4096];

	va_start (argptr, format);
	Q_vsprintf (string, format, argptr);
	va_end (argptr);

	Com_Printf("ERROR: %s, line %d: %s\n", com_parsename, com_lines, string);
}


void COM_ParseWarning( char *format, ... )
{
	va_list argptr;
	static char string[4096];

	va_start (argptr, format);
	Q_vsprintf (string, format, argptr);
	va_end (argptr);

	Com_Printf("WARNING: %s, line %d: %s\n", com_parsename, com_lines, string);
}


/*
==============
COM_Parse

Parse a token out of a string
Will never return NULL, just empty strings

If "allowLineBreaks" is qtrue then an empty
string will be returned if the next token is
a newline.
==============
*/
static char *SkipWhitespace( char *data, qboolean *hasNewLines ) {
	int c;

	while( (c = *data) <= ' ') {
		if( !c ) {
			return NULL;
		}
		if( c == '\n' ) {
			com_lines++;
			*hasNewLines = qtrue;
		}
		data++;
	}

	return data;
}


int COM_Compress( char *data_p ) {
	char *in, *out;
	int c;
	qboolean newline = qfalse, whitespace = qfalse;

	in = out = data_p;
	if (in) {
		while ((c = *in) != 0) {
			// skip double slash comments
			if ( c == '/' && in[1] == '/' ) {
				while (*in && *in != '\n') {
					in++;
				}
			// skip /* */ comments
			} else if ( c == '/' && in[1] == '*' ) {
				while ( *in && ( *in != '*' || in[1] != '/' ) ) 
					in++;
				if ( *in ) 
					in += 2;
                        // record when we hit a newline
                        } else if ( c == '\n' || c == '\r' ) {
                            newline = qtrue;
                            in++;
                        // record when we hit whitespace
                        } else if ( c == ' ' || c == '\t') {
                            whitespace = qtrue;
                            in++;
                        // an actual token
			} else {
                            // if we have a pending newline, emit it (and it counts as whitespace)
                            if (newline) {
                                *out++ = '\n';
                                newline = qfalse;
                                whitespace = qfalse;
                            } else if (whitespace) {
                                *out++ = ' ';
                                whitespace = qfalse;
                            }
                            
                            // copy quoted strings unmolested
                            if (c == '"') {
                                    *out++ = c;
                                    in++;
                                    while (1) {
                                        c = *in;
                                        if (c && c != '"') {
                                            *out++ = c;
                                            in++;
                                        } else {
                                            break;
                                        }
                                    }
                                    if (c == '"') {
                                        *out++ = c;
                                        in++;
                                    }
                            } else {
                                *out = c;
                                out++;
                                in++;
                            }
			}
		}
	}
	*out = 0;
	return out - data_p;
}


char *COM_ParseExt( char **data_p, qboolean allowLineBreaks )
{
	int c = 0, len;
	qboolean hasNewLines = qfalse;
	char *data;

	data = *data_p;
	len = 0;
	com_token[0] = '\0';
	com_tokenline = 0;

	// make sure incoming data is valid
	if ( !data )
	{
		*data_p = NULL;
		return com_token;
	}

	while ( 1 )
	{
		// skip whitespace
		data = SkipWhitespace( data, &hasNewLines );
		if ( !data )
		{
			*data_p = NULL;
			return com_token;
		}
		if ( hasNewLines && !allowLineBreaks )
		{
			*data_p = data;
			return com_token;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while (*data && *data != '\n') {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c == '/' && data[1] == '*' ) 
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
			{
				if ( *data == '\n' )
				{
					com_lines++;
				}
				data++;
			}
			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	com_tokenline = com_lines;

	// handle quoted strings
	if ( c == '"' )
	{
		data++;
		while ( 1 )
		{
			c = *data;
			if ( c == '"' || c == '\0' )
			{
				if ( c == '"' )
					data++;

				com_token[ len ] = '\0';
				*data_p = ( char * ) data;
				return com_token;
			}
			data++;
			if ( c == '\n' )
			{
				com_lines++;
			}
			if ( len < MAX_TOKEN_CHARS-1 )
			{
				com_token[ len ] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if ( len < MAX_TOKEN_CHARS - 1 )
		{
			com_token[ len ] = c;
			len++;
		}
		data++;
		c = *data;
	} while ( c > ' ' );

	com_token[ len ] = '\0';

	*data_p = ( char * ) data;
	return com_token;
}


/*
==================
COM_MatchToken
==================
*/
void COM_MatchToken( char **buf_p, char *match ) {
	char	*token;

	token = COM_Parse( buf_p );
	if ( strcmp( token, match ) ) {
		Com_Error( ERR_DROP, "MatchToken: %s != %s", token, match );
	}
}


/*
=================
SkipBracedSection

The next token should be an open brace.
Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
void SkipBracedSection( char **program ) {
	char			*token;
	int				depth;

	depth = 0;
	do {
		token = COM_ParseExt( program, qtrue );
		if( token[1] == 0 ) {
			if( token[0] == '{' ) {
				depth++;
			}
			else if( token[0] == '}' ) {
				depth--;
			}
		}
	} while( depth && *program );
}


/*
=================
SkipRestOfLine
=================
*/
void SkipRestOfLine( char **data ) {
	char	*p;
	int		c;

	p = *data;

	if ( !*p )
		return;

	while ( (c = *p) != '\0' ) {
		p++;
		if ( c == '\n' ) {
			com_lines++;
			break;
		}
	}

	*data = p;
}


void Com_InitSeparators( void )
{
	is_separator['\n']=1;
	is_separator[';']=1;
	is_separator['=']=1;
	is_separator['{']=1;
	is_separator['}']=1;
}


void SkipTillSeparators( char **data )
{
	char	*p;
	int	c;

	p = *data;

	if ( !*p )
		return;

	while ( (c = *p) != '\0' ) 
	{
		p++;
		if ( is_separator[ c ] )
		{
			if ( c == '\n' )
			{
				com_lines++;
			}
			break;
		}
	}

	*data = p;
}


char *COM_ParseSep( char **data_p, qboolean allowLineBreaks )
{
	int c = 0, len;
	qboolean hasNewLines = qfalse;
	char *data;

	data = *data_p;
	len = 0;
	com_token[0] = '\0';
	com_tokenline = 0;

	// make sure incoming data is valid
	if ( !data )
	{
		*data_p = NULL;
		return com_token;
	}

	while ( 1 )
	{
		// skip whitespace
		data = SkipWhitespace( data, &hasNewLines );
		if ( !data )
		{
			*data_p = NULL;
			return com_token;
		}
		if ( hasNewLines && !allowLineBreaks )
		{
			*data_p = data;
			return com_token;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while (*data && *data != '\n') {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c == '/' && data[1] == '*' ) 
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
			{
				if ( *data == '\n' )
				{
					com_lines++;
				}
				data++;
			}
			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	com_tokenline = com_lines;

	// handle quoted strings
	if ( c == '"' )
	{
		data++;
		while ( 1 )
		{
			c = *data;
			if ( c == '"' || c == '\0' )
			{
				if ( c == '"' )
					data++;

				com_token[ len ] = '\0';
				*data_p = ( char * ) data;
				return com_token;
			}
			data++;
			if ( c == '\n' )
			{
				com_lines++;
			}
			if ( len < MAX_TOKEN_CHARS-1 )
			{
				com_token[ len ] = c;
				len++;
			}
		}
	}

	// special case for separators
 	if ( is_separator[ c ]  )  
	{
		com_token[ len ] = c;
		len++;
		data++;
	} 
	else // parse a regular word
	do
	{
		if ( len < MAX_TOKEN_CHARS - 1 )
		{
			com_token[ len ] = c;
			len++;
		}
		data++;
		c = *data;
	} while ( c > ' ' && !is_separator[ c ] );

	com_token[ len ] = '\0';

	*data_p = ( char * ) data;
	return com_token;
}


/*
============
Com_Split
============
*/
int Com_Split( char *in, char **out, int outsz, int delim ) 
{
	int c;
	char **o = out, **end = out + outsz;
	// skip leading spaces
	if ( delim >= ' ' ) {
		while( (c = *in) != '\0' && c <= ' ' ) 
			in++; 
	}
	*out = in; out++;
	while( out < end ) {
		while( (c = *in) != '\0' && c != delim ) 
			in++; 
		*in = '\0';
		if ( !c ) {
			// don't count last null value
			if ( out[-1][0] == '\0' ) 
				out--;
			break;
		}
		in++;
		// skip leading spaces
		if ( delim >= ' ' ) {
			while( (c = *in) != '\0' && c <= ' ' ) 
				in++; 
		}
		*out = in; out++;
	}
	// sanitize last value
	while( (c = *in) != '\0' && c != delim ) 
		in++; 
	*in = '\0';
	c = out - o;
	// set remaining out pointers
	while( out < end ) {
		*out = in; out++;
	}
	return c;
}


void Parse1DMatrix (char **buf_p, int x, float *m) {
	char	*token;
	int		i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < x ; i++) {
		token = COM_Parse(buf_p);
		m[i] = atof(token);
	}

	COM_MatchToken( buf_p, ")" );
}

void Parse2DMatrix (char **buf_p, int y, int x, float *m) {
	int		i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < y ; i++) {
		Parse1DMatrix (buf_p, x, m + i * x);
	}

	COM_MatchToken( buf_p, ")" );
}

void Parse3DMatrix (char **buf_p, int z, int y, int x, float *m) {
	int		i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < z ; i++) {
		Parse2DMatrix (buf_p, y, x, m + i * x*y);
	}

	COM_MatchToken( buf_p, ")" );
}


/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

const byte locase[ 256 ] =
{
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
	0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
	0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
	0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f,
	0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
	0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
	0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
	0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
	0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
	0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
	0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
	0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
	0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
	0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
	0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
	0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
	0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
	0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
	0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
	0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
	0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};


int Q_isprint( int c )
{
	if ( c >= 0x20 && c <= 0x7E )
		return ( 1 );
	return ( 0 );
}


int Q_islower( int c )
{
	if (c >= 'a' && c <= 'z')
		return ( 1 );
	return ( 0 );
}


int Q_isupper( int c )
{
	if (c >= 'A' && c <= 'Z')
		return ( 1 );
	return ( 0 );
}


int Q_isalpha( int c )
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return ( 1 );
	return ( 0 );
}


char* Q_strrchr( const char* string, int c )
{
	char cc = c;
	char *s;
	char *sp=(char *)0;

	s = (char*)string;

	while (*s)
	{
		if (*s == cc)
			sp = s;
		s++;
	}
	if (cc == 0)
		sp = s;

	return sp;
}


/*
=============
Q_strncpyz
 
Safe strncpy that ensures a trailing zero
=============
*/
void Q_strncpyz( char *dest, const char *src, int destsize ) 
{
	if ( !dest ) {
		Com_Error( ERR_FATAL, "Q_strncpyz: NULL dest" );
	}

	if ( !src ) {
		Com_Error( ERR_FATAL, "Q_strncpyz: NULL src" );
	}

	if ( destsize < 1 ) {
		Com_Error(ERR_FATAL,"Q_strncpyz: destsize < 1" ); 
	}

	strncpy( dest, src, destsize-1 );
	dest[ destsize-1 ] = '\0';
}

                 
int Q_stricmpn( const char *s1, const char *s2, int n ) {
	int	c1, c2;

	if ( s1 == NULL ) {
		if ( s2 == NULL )
			return 0;
		else
			return -1;
	} else if ( s2 == NULL )
		return 1;
	
	do {
		c1 = *s1; s1++;
		c2 = *s2; s2++;

		if ( !n-- ) {
			return 0; // strings are equal until end point
		}
		
		if ( c1 != c2 ) {
			if (c1 >= 'a' && c1 <= 'z') {
				c1 -= ('a' - 'A');
			}
			if (c2 >= 'a' && c2 <= 'z') {
				c2 -= ('a' - 'A');
			}
			if (c1 != c2) {
				return c1 < c2 ? -1 : 1;
			}
		}
	} while (c1);
	
	return 0;		// strings are equal
}


int Q_strncmp( const char *s1, const char *s2, int n ) {
	int		c1, c2;
	
	do {
		c1 = *s1; s1++;
		c2 = *s2; s2++;

		if (!n--) {
			return 0;		// strings are equal until end point
		}
		
		if (c1 != c2) {
			return c1 < c2 ? -1 : 1;
		}
	} while (c1);
	
	return 0;		// strings are equal
}


int Q_stricmp( const char *s1, const char *s2 ) 
{
	unsigned char c1, c2;

	if ( s1 == NULL ) 
	{
		if ( s2 == NULL )
			return 0;
		else
			return -1;
	}
	else if ( s2 == NULL )
		return 1;
	
	do 
	{
		c1 = *s1; s1++;
		c2 = *s2; s2++;

		if ( c1 != c2 ) 
		{
			if ( c1 <= 'Z' && c1 >= 'A' )
				c1 += ('a' - 'A');

			if ( c2 <= 'Z' && c2 >= 'A' )
				c2 += ('a' - 'A');

			if ( c1 != c2 ) 
				return c1 < c2 ? -1 : 1;
		}
	}
	while ( c1 != '\0' );

	return 0;
}


char *Q_strlwr( char *s1 ) 
{
    char	*s;

    s = s1;
	while ( *s ) {
		*s = tolower(*s);
		s++;
	}
    return s1;
}


char *Q_strupr( char *s1 ) {
    char	*s;

    s = s1;
	while ( *s ) {
		*s = toupper(*s);
		s++;
	}
    return s1;
}


// never goes past bounds or leaves without a terminating 0
void Q_strcat( char *dest, int size, const char *src ) {
	int		l1;

	l1 = strlen( dest );
	if ( l1 >= size ) {
		Com_Error( ERR_FATAL, "Q_strcat: already overflowed" );
	}
	Q_strncpyz( dest + l1, src, size - l1 );
}


int Q_PrintStrlen( const char *string ) {
	int			len;
	const char	*p;

	if( !string ) {
		return 0;
	}

	len = 0;
	p = string;
	while( *p ) {
		if( Q_IsColorString( p ) ) {
			p += 2;
			continue;
		}
		p++;
		len++;
	}

	return len;
}


char *Q_CleanStr( char *string ) {
	char*	d;
	char*	s;
	int		c;

	s = string;
	d = string;
	while ((c = *s) != '\0' ) {
		if ( Q_IsColorString( s ) ) {
			s++;
		}
		else if ( c >= ' ' && c <= '~' ) {
			*d = c; d++;
		}
		s++;
	}
	*d = '\0';

	return string;
}


int QDECL Com_sprintf( char *dest, int size, const char *fmt, ... ) {
	va_list argptr;
	int len;

	va_start( argptr, fmt );
	len = Q_vsprintf( dest, fmt, argptr );
	va_end( argptr );

	if ( len >= size ) {
		Com_Error( ERR_FATAL, "Com_sprintf: overflow of %i in %i\n", len, size );
	}

	return len;
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char * QDECL va( const char *format, ... ) 
{
	va_list		argptr;
	static char		string[2][32000];	// in case va is called by nested functions
	static int		index = 0;
	char	*buf;

	buf = string[ index ];
	index ^= 1;

	va_start( argptr, format );
	Q_vsprintf( buf, format, argptr );
	va_end( argptr );

	return buf;
}


/*
=====================================================================

  INFO STRINGS

=====================================================================
*/


static qboolean Q_strkey( const char *str, const char *key, int key_len )
{
	int i;

	for ( i = 0; i < key_len; i++ )
	{
		if ( locase[ (byte)str[i] ] != locase[ (byte)key[i] ] )
		{
			return qfalse;
		}
	}

	return qtrue;
}


/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey( const char *s, const char *key ) {
	static	char value[2][BIG_INFO_VALUE];	// use two buffers so compares
											// work without stomping on each other
	static	int	valueindex = 0;
	const char *v, *pkey;
	char	*o, *o2;
	int		klen, len;

	if ( !s || !key || !*key )
		return "";

	klen = (int)strlen( key );

	if ( *s == '\\' )
		s++;

	while (1)
	{
		pkey = s;
		while ( *s != '\\' )
		{
			if ( *s == '\0' )
				return "";
			++s;
		}
		len = (int)(s - pkey);
		s++; // skip '\\'

		v = s;
		while ( *s != '\\' && *s !='\0' )
			s++;

		if ( len == klen && Q_strkey( pkey, key, klen ) )
		{
			o = o2 = value[ valueindex ];
			valueindex ^= 1;
			if ( (int)(s - v) >= BIG_INFO_STRING )
			{
				Com_Error( ERR_DROP, "Info_ValueForKey: oversize infostring value" );
			}
			else 
			{
				while ( v < s )
				{
					*o = *v;
					++o; ++v;
				}
			}
			*o = '\0';
			return o2;
		}

		if ( *s == '\0' )
			break;

		s++;
	}

	return "";
}


/*
===================
Info_NextPair

Used to itterate through all the key/value pairs in an info string
===================
*/
const char *Info_NextPair( const char *s, char *key, char *value ) {
	char *o;

	if ( *s == '\\' ) {
		s++;
	}

	key[0] = '\0';
	value[0] = '\0';

	o = key;
	while ( *s != '\\' ) {
		if ( *s == '\0' ) {
			*o = '\0';
			return s;
		}
		*o++ = *s++;
	}
	*o = '\0';
	s++;

	o = value;
	while ( *s != '\\' && *s != '\0' ) {
		*o++ = *s++;
	}
	*o = '\0';

	return s;
}


/*
===================
Info_RemoveKey
===================
*/
static int Info_RemoveKey( char *s, const char *key ) {
	char	*start;
	char 	*pkey;
	int		key_len, len;

	key_len = (int) strlen( key );

	while ( 1 )
	{
		start = s;
		if ( *s == '\\' )
			s++;
		pkey = s;
		while ( *s != '\\' )
		{
			if ( *s == '\0' )
				return 0;
			++s;
		}

		len = (int)(s - pkey);
		++s; // skip '\\'

		while ( *s != '\\' && *s != '\0' )
			++s;

		if ( len == key_len && Q_strkey( pkey, key, key_len ) )
		{
			memmove( start, s, strlen( s ) + 1 ); // remove this part
			return (int)(s - start);
		}

		if ( *s == '\0' )
			break;
	}

	return 0;
}


/*
==================
Info_ValidateKeyValue

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate( const char *s )
{
	for ( ;; )
	{
		switch ( *s )
		{
		case '\0':
			return qtrue;
		case '\"':
		case ';':
			return qfalse;
		default:
			++s;
			continue;
		}
	}
}


/*
==================
Info_ValidateKeyValue
==================
*/
qboolean Info_ValidateKeyValue( const char *s )
{
	for ( ;; )
	{
		switch ( *s )
		{
		case '\0':
			return qtrue;
		case '\\':
		case '\"':
		case ';':
			return qfalse;
		default:
			++s;
			continue;
		}
	}
}


/*
==================
Info_SetValueForKey

Changes or adds a key/value pair
==================
*/
qboolean Info_SetValueForKey( char *s, const char *key, const char *value ) {
	char	newi[MAX_INFO_STRING+2];
	int		len1, len2;

	len1 = (int)strlen( s );
	if ( len1 >= MAX_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_SetValueForKey: oversize infostring" );
	}

	if ( !Info_ValidateKeyValue( key ) || *key == '\0' ) {
		Com_Printf( S_COLOR_YELLOW "Invalid key name: '%s'\n", key );
		return qfalse;
	}

	if ( !Info_ValidateKeyValue( value ) ) {
		Com_Printf( S_COLOR_YELLOW "Invalid value name: '%s'\n", value );
		return qfalse;
	}

	len1 -= Info_RemoveKey( s, key );
	if ( !value || !*value )
		return qtrue;

	len2 = Com_sprintf( newi, sizeof( newi ), "\\%s\\%s", key, value );
	
	if ( len1 + len2 >= MAX_INFO_STRING )
	{
		Com_Printf( S_COLOR_YELLOW "Info string length exceeded\n" );
		return qfalse;
	}

	strcpy( s + len1, newi );
	return qtrue;
}


/*
==================
Info_SetValueForKey_Big

Changes or adds a key/value pair
==================
*/
qboolean Info_SetValueForKey_Big( char *s, const char *key, const char *value ) {
	char	newi[BIG_INFO_STRING+2];
	int		len1, len2;

	len1 = (int)strlen( s );
	if ( len1 >= BIG_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_SetValueForKey: oversize infostring" );
	}

	if ( !Info_ValidateKeyValue( key ) || *key == '\0' ) {
		Com_Printf( S_COLOR_YELLOW "Invalid key name: '%s'\n", key );
		return qfalse;
	}

	if ( !Info_ValidateKeyValue( value ) ) {
		Com_Printf( S_COLOR_YELLOW "Invalid value name: '%s'\n", value );
		return qfalse;
	}

	len1 -= Info_RemoveKey( s, key );
	if ( !value || !*value )
		return qtrue;

	len2 = Com_sprintf( newi, sizeof( newi ), "\\%s\\%s", key, value );

	if ( len1 + len2 >= BIG_INFO_STRING )
	{
		Com_Printf( S_COLOR_YELLOW "BIG Info string length exceeded\n" );
		return qfalse;
	}

	strcpy( s + len1, newi );
	return qtrue;
}
