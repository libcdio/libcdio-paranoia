/*
  Copyright (C) 2008 Rocky Bernstein <rocky@gnu.org>
*/

extern int verbose;
extern int quiet;
extern FILE *reportfile;

#define report(...) {if(!quiet){fprintf(stderr, __VA_ARGS__);fputc('\n',stderr);} \
    if(reportfile){fprintf(reportfile, __VA_ARGS__);fputc('\n',reportfile);}}
#define reportC(...) {if(!quiet){fprintf(stderr, __VA_ARGS__);}	\
    if(reportfile){fprintf(reportfile, __VA_ARGS__);}}
#define printC(...) {if(!quiet){fprintf(stderr, __VA_ARGS__);}}
#define logC(...) {if(reportfile){fprintf(reportfile, __VA_ARGS__);}}
