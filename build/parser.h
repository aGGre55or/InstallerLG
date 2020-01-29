/* A Bison parser, made by GNU Bison 3.5.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2019 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

#ifndef YY_YY_PARSER_H_INCLUDED
# define YY_YY_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    SYM = 258,
    STR = 259,
    OOM = 260,
    INT = 261,
    HEX = 262,
    BIN = 263,
    EQ = 264,
    GT = 265,
    GTE = 266,
    LT = 267,
    LTE = 268,
    NEQ = 269,
    IF = 270,
    SELECT = 271,
    UNTIL = 272,
    WHILE = 273,
    TRACE = 274,
    RETRACE = 275,
    EXECUTE = 276,
    REXX = 277,
    RUN = 278,
    ABORT = 279,
    EXIT = 280,
    ONERROR = 281,
    TRAP = 282,
    REBOOT = 283,
    COPYFILES = 284,
    COPYLIB = 285,
    DELETE = 286,
    EXISTS = 287,
    FILEONLY = 288,
    FOREACH = 289,
    MAKEASSIGN = 290,
    MAKEDIR = 291,
    PROTECT = 292,
    STARTUP = 293,
    TEXTFILE = 294,
    TOOLTYPE = 295,
    TRANSCRIPT = 296,
    RENAME = 297,
    COMPLETE = 298,
    DEBUG = 299,
    MESSAGE = 300,
    USER = 301,
    WELCOME = 302,
    WORKING = 303,
    AND = 304,
    BITAND = 305,
    BITNOT = 306,
    BITOR = 307,
    BITXOR = 308,
    NOT = 309,
    IN = 310,
    OR = 311,
    SHIFTLEFT = 312,
    SHIFTRIGHT = 313,
    XOR = 314,
    CLOSEMEDIA = 315,
    EFFECT = 316,
    SETMEDIA = 317,
    SHOWMEDIA = 318,
    DATABASE = 319,
    EARLIER = 320,
    GETASSIGN = 321,
    GETDEVICE = 322,
    GETDISKSPACE = 323,
    GETENV = 324,
    GETSIZE = 325,
    GETSUM = 326,
    GETVERSION = 327,
    ICONINFO = 328,
    QUERYDISPLAY = 329,
    CUS = 330,
    DCL = 331,
    ASKBOOL = 332,
    ASKCHOICE = 333,
    ASKDIR = 334,
    ASKDISK = 335,
    ASKFILE = 336,
    ASKNUMBER = 337,
    ASKOPTIONS = 338,
    ASKSTRING = 339,
    CAT = 340,
    EXPANDPATH = 341,
    FMT = 342,
    PATHONLY = 343,
    PATMATCH = 344,
    STRLEN = 345,
    SUBSTR = 346,
    TACKON = 347,
    SET = 348,
    SYMBOLSET = 349,
    SYMBOLVAL = 350,
    OPENWBOBJECT = 351,
    SHOWWBOBJECT = 352,
    CLOSEWBOBJECT = 353,
    ALL = 354,
    APPEND = 355,
    ASSIGNS = 356,
    BACK = 357,
    CHOICES = 358,
    COMMAND = 359,
    COMPRESSION = 360,
    CONFIRM = 361,
    DEFAULT = 362,
    DELOPTS = 363,
    DEST = 364,
    DISK = 365,
    FILES = 366,
    FONTS = 367,
    GETDEFAULTTOOL = 368,
    GETPOSITION = 369,
    GETSTACK = 370,
    GETTOOLTYPE = 371,
    HELP = 372,
    INFOS = 373,
    INCLUDE = 374,
    NEWNAME = 375,
    NEWPATH = 376,
    NOGAUGE = 377,
    NOPOSITION = 378,
    NOREQ = 379,
    PATTERN = 380,
    PROMPT = 381,
    QUIET = 382,
    RANGE = 383,
    SAFE = 384,
    SETDEFAULTTOOL = 385,
    SETPOSITION = 386,
    SETSTACK = 387,
    SETTOOLTYPE = 388,
    SOURCE = 389,
    SWAPCOLORS = 390,
    OPTIONAL = 391,
    RESIDENT = 392,
    OVERRIDE = 393
  };
#endif

/* Value type.  */



int yyparse (yyscan_t scanner);

#endif /* !YY_YY_PARSER_H_INCLUDED  */
