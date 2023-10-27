#include<fakeLisp/regex.h>
#include<fakeLisp/utils.h>
#include<string.h>

static inline void print_match_res(const char* str,uint32_t len,uint32_t pos)
{
	printf("\n%s %u %u\n",str,len,pos);
	if(len)
	{
		for(uint32_t i=0;i<pos;i++)
			putchar(' ');
		putchar('^');
		for(uint32_t i=2;i<len;i++)
			putchar('-');
		putchar('^');
		putchar('\n');
	}
}

#define MATCH_INT "^[-+]?(0|[1-9]\\d*)$"
// #define MATCH_STRING "^\"(\\\\.|[^\"\\\\])*\"$"
#define MATCH_STRING "^\"(\\\\.|.)*\"$"
#define MATCH_BRANCH "(ab|cd|ef)"
#define MATCH_UTF_STRING "^“(\\\\.|.)*”$"

static const struct PatternAndText
{
	const char* pattern;
	const char* text;
	uint32_t lex;
	uint32_t len;
	union
	{
		uint32_t pos;
		int last_is_true;
	};
}pattern_and_str[]=
{
	{MATCH_INT,        "114514",       0, 6,                   .pos=0,          },
	{MATCH_INT,        "000000",       0, 7,                   .pos=0,          },
	{MATCH_INT,        "0",            0, 1,                   .pos=0,          },
	{"\\x20*",         "    ",         0, 4,                   .pos=0,          },
	{"[\\x20]*",       "    ",         0, 4,                   .pos=0,          },
	{MATCH_STRING,     "\"ab\\\"cd\"", 0, 8,                   .pos=0,          },
	{MATCH_STRING,     "\"ab\"cd",     1, 4,                   .last_is_true=1, },
	{MATCH_STRING,     "\"ab\\\"cd",   1, 8,                   .last_is_true=1, },
	{MATCH_STRING,     "\"ab\\\"cd\"", 1, 8,                   .last_is_true=1, },
	{MATCH_STRING,     "\"ab\\",       0, 5,                   .pos=0,          },
	{"\\(a+\\)",       "(",            1, 2,                   .last_is_true=1, },
	{"\\(a+\\)",       "(a",           1, 3,                   .last_is_true=1, },
	{"ab+$",           "abbc",         1, 3,                   .last_is_true=1, },
	{"ab+$",           "",             1, 1,                   .last_is_true=0, },
	{".*",             "",             1, 0,                   .last_is_true=1, },
	{"^/\\*.*(\\*/)$", "/* * */",      1, 7,                   .last_is_true=1, },
	{MATCH_BRANCH,     "abcd",         0, 2,                   .pos=0,          },
	{MATCH_BRANCH,     "cd",           0, 2,                   .pos=0,          },
	{MATCH_UTF_STRING, "“cd”",         0, sizeof("“cd”")-1,    .pos=0,          },
	{MATCH_UTF_STRING, "“c\\”d”",      0, sizeof("“c\\”d”")-1, .pos=0,          },

	{NULL,             NULL,           0, 0,                   .pos=0,          },
};

int main()
{
	for(const struct PatternAndText* cur=&pattern_and_str[0];cur->pattern;cur++)
	{
		fputs("====\n",stdout);
		const char* pattern=cur->pattern;
		const char* str=cur->text;
		uint32_t pattern_len=strlen(pattern);
		FklRegexCode* re=fklRegexCompileCharBuf(pattern,pattern_len);
		FKL_ASSERT(re);
		fklRegexPrint(re,stdout);
		uint32_t pos=0;
		fklRegexPrintAsC(re,NULL,pattern,pattern_len,stdout);
		uint32_t str_len=strlen(str);
		int last_is_true=0;
		uint32_t len=cur->lex
			?fklRegexLexMatchp(re,str,str_len,&last_is_true)
			:fklRegexMatchpInCharBuf(re,str,str_len,&pos);
		if(cur->lex)
			FKL_ASSERT(len==cur->len&&last_is_true==cur->last_is_true);
		else
			FKL_ASSERT(len==cur->len&&pos==cur->pos);
		if(len<=str_len)
			print_match_res(str,len,pos);
		fklRegexFree(re);
		fputc('\n',stdout);
	}
	return 0;
}

