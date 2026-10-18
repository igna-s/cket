#include <quantum.h>
int main(void) { quant bool a=superposition(0,1),b=false; if(a)b^=true; unsigned flag=measure(a); return measure(b)!=flag; }
