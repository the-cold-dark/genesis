/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_strutil_h
#define cdc_strutil_h

void     init_match(void);
cList * match_template(char * ctemplate, char * s);
cList * match_pattern(char * pattern, char * s);
cList * match_regexp(cStr * reg, char * s, Bool sensitive, Bool * error);
cList * regexp_matches(cStr * reg, char * s, Bool sensitive, Bool * error);
Int parse_regfunc_args(char * args, Int flags);
cStr * strsub(cStr * sstr, cStr * ssearch, cStr * sreplace, Int flags);
cStr * strsed(cStr * reg,      /* the regexp string */
                  cStr * ss,   /* the string to match against */
                  cStr * rs,   /* the replacement string */
                  Int flags,   /* flags */
                  Int mult);   /* multiplier */
cStr * strfmt(cStr * str, cData * args, Int argc);
cList   * strexplode(cStr * str, char * sep, Int sep_len, Bool blanks);
cList   * strsplit(cStr * str, cStr * regexp, Int flags);

/* string flags */
#define RF_NONE      0
#define RF_GLOBAL    1
#define RF_SENSITIVE 2
#define RF_BLANKS    4

#endif

