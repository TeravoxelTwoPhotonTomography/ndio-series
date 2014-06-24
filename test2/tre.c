#include <tre/tre.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define assert(e) do{if(!(e)) {printf("%s(%d): %s()\n\tExpression failed.\n\t%s\n",__FILE__,__LINE__,__FUNCTION__,#e);/* breakme();*/ failed; }}while(0)
#define failed    do{printf("FAILED -- %s\n",__FUNCTION__); exit(1);}while(0)
#define passed    printf("PASSED -- %s\n",__FUNCTION__);

void breakme() {failed;};

int expect(const regmatch_t *m,const char* expected, const char* source) {
  int n=m->rm_eo-m->rm_so;
  return 
    (strlen(expected)==n) && 
    (strncmp(expected,source+m->rm_so,n)==0);
}

void test1() {	
	char* name="/b/vol.0.0000.tif";
	regex_t p;
	regmatch_t m;
  char t;
	assert(tre_regcomp(&p,"\\d+",REG_EXTENDED)==0);
	assert(tre_regexec(&p,name,1,&m,0)==0);

  t=name[m.rm_eo];
	name[m.rm_eo]='\0';
	printf("Match: %s\n",name+m.rm_so);
  name[m.rm_eo]=t;

  name+=m.rm_eo;
  assert(tre_regexec(&p,name,1,&m,0)==0);
	name[m.rm_eo]='\0';
	printf("Match: %s\n",name+m.rm_so);
  passed;
}

#define countof(e) (sizeof(e)/sizeof(*(e)))

/* The Lesson Here
is that the first match is the fully matched substring and the
captured groups come after that.
*/
void test2() {
  char* name="/b/vol.0.0123.tif";
	regex_t p;
  regmatch_t m[10]={0};
  char t;
  int i;
	assert(tre_regcomp(&p,"/b/vol\\.([[:digit:]]+)\\.([[:digit:]]+).tif",REG_EXTENDED)==0);  
	assert(tre_regexec(&p,name,countof(m),m,0)==0);

  for(i=0;i<countof(m);++i) {
    if(m[i].rm_so<0) continue;
    t=name[m[i].rm_eo];
	  name[m[i].rm_eo]='\0';
	  printf("Match %3d: %s\n",i,name+m[i].rm_so);
    name[m[i].rm_eo]=t;
  }

  assert(expect(&m[1],"0",name));
  assert(expect(&m[2],"0123",name));
  
  passed;
}

void test3() {
  char* name="/a/vol.1ch%.tif";
  regex_t p;
  regmatch_t m[10]={0};
  char t;
  int i;
  assert(tre_regcomp(&p,"%+",REG_EXTENDED)==0);  
	assert(tre_regexec(&p,name,countof(m),m,0)==0);

  for(i=0;i<countof(m);++i) {
    if(m[i].rm_so<0) continue;
    t=name[m[i].rm_eo];
	  name[m[i].rm_eo]='\0';
	  printf("Match %3d: %s\n",i,name+m[i].rm_so);
    name[m[i].rm_eo]=t;
  }
  assert(m[0].rm_so==10);
  assert(m[0].rm_eo==11);
  passed;
}


int main(int argc,char* argv[]) {
	test1();
  test2();
  test3();
	return 0;
}
