#include"form.h"
int N_quote(cell* objCell,env* curEnv)
{
	consPair* objCons=objCell->prev;
	if(objCons==NULL)objCell->type=str;
	else
	{
		if(objCons->right.type==nil)return 1;
		else if(objCons->right.type==atm)return 1;
		else 
		{
			consPair* argCons=objCons->right.value;
			free(objCons->left.value);
			objCons->left.type=argCons->left.type;
			objCons->left.value=argCons->left.value;
			if(argCons->left.type==con)
				(consPair*)(argCons->left.value)->prev=objCons;
			else if(argCons->left.type==atm)
				(atom*)(argCons->left.value)->prev=objCons;
			objCons->right.type=argCons->right.type;
			objCons->right.value=argCons->right.value;
			if(argCons->right.type==con)
				(consPair*)(argCons->right.value)->prev=objCons;
			else if(argCons->right.type==atm)
				(atom*)(argCons->right.value)->prev=objCons;
			free(argCons);
		}
	}
	return 0;
}

int N_car(cell* objCell,env* curEnv)
{
	consPair* objCons=objCell->prev;
	if(objCons==NULL)objCell->type=str;
	else
	{
		if(objCons->right.type==nil)return 1;
		else if(objCons->right.type==atm)return 1;
		else
		{
			
		}
	}
	return 0;
}
