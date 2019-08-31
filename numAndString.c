#include<stdlib.h>
#include<stdio.h>
#include<string.h>
char* floatToString(double num)
{
	int i;
	char numString[sizeof(double)*2+3];
	sprintf(numString,"%lf",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	for(i=0;i<lenOfNum;i++)*(tmp+i)=numString[i];
	return tmp;
}
double stringToFloat(char* str)
{
	double tmp;
	sscanf(str,"%lf",&tmp);
	return tmp;
}
char* intToString(long num)
{
	int i;
	char numString[sizeof(long)*2+3];
	sprintf(numString,"%ld",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	for(i=0;i<lenOfNum;i++)*(tmp+i)=numString[i];
	return tmp;
}
long stringToInt(char* str)
{
	long tmp;
	sscanf(str,"%ld",&tmp);
	return tmp;
}
