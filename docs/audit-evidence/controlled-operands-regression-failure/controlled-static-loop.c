#include <quantum.h>
int main(void) { quant bool c=1; quant uint8_t x=0; for(int i=0;i<3;++i)if(c)x+=i+1; int a=measure(x),b=measure(c); return a!=6||b!=1; }
