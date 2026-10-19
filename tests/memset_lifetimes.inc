/* Scope cleanup must preserve relative phases and saved CPU operands.
 * The final three source-order cases keep existing observation/transfer behavior.
 * Included in native_cases; each fixture runs at five levels and both builtin modes. */
{"lifetime-quantum-byte",
 "#include <quantum.h>\n"
 "#include <string.h>\n"
 "int main(void){quant bool b=superposition(0,1);{quant uint8_t x=5;memset((void*)&x,b,1);if(x==1)phase(3.14159265358979323846);}inverse_fourier(b);return measure(b)!=1;}\n",
 NULL,
 1},
    {"lifetime-partial-word",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){quant bool b=superposition(0,1);{quant uint16_t x=0x1234;memset((char*)&x+1,b,1);if(x==0x134)phase(3.14159265358979323846);}inverse_fourier(b);return measure(b)!=1;}\n",
     NULL,
     1},
    {"lifetime-array",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){quant bool b=superposition(0,1);{quant uint8_t x[2]={5,6};if(b)memset((void*)x,7,2);if(x[0]==7&&x[1]==7)phase(3.14159265358979323846);}inverse_fourier(b);return measure(b)!=1;}\n",
     NULL,
     1},
    {"lifetime-signed",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){quant bool b=superposition(0,1);{quant int8_t x=-5;if(b)memset((void*)&x,-1,1);if(x==-1)phase(3.14159265358979323846);}inverse_fourier(b);return measure(b)!=1;}\n",
     NULL,
     1},
    {"lifetime-clean-zero",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){quant bool b=superposition(0,1);{quant uint8_t x=0;memset((void*)&x,b,1);if(x==1)phase(3.14159265358979323846);}inverse_fourier(b);return measure(b)!=1;}\n",
     NULL,
     1},
    {"lifetime-cpu-arm-true",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){volatile int flag=1;quant bool b=superposition(0,1);{quant uint8_t x=5;if(flag){if(b)memset((void*)&x,7,1);}if(x==7)phase(3.14159265358979323846);}inverse_fourier(b);return measure(b)!=1;}\n",
     NULL,
     1},
    {"lifetime-cpu-arm-false",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){volatile int flag=0;quant bool b=superposition(0,1);{quant uint8_t x=5;if(flag){if(b)memset((void*)&x,7,1);}if(x==7)phase(3.14159265358979323846);}inverse_fourier(b);return measure(b)!=0;}\n",
     NULL,
     1},
    {"lifetime-observe-source-first",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset((void*)&x,b,1);unsigned a=measure(b),v=measure(x);return a!=v;}\n",
     NULL,
     1},
    {"lifetime-move-source-first",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset((void*)&x,b,1);quant uint8_t y=x;unsigned a=measure(b),v=measure(y);return a!=v;}\n",
     NULL,
     1},
    {"lifetime-transform-source-first",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){quant bool b=superposition(0,1);quant uint8_t x=5;memset((void*)&x,b,1);fourier(x);inverse_fourier(x);unsigned a=measure(b),v=measure(x);return a!=v;}\n",
     NULL,
     1},
    {"lifetime-mutable-cpu",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){int value=7;quant bool b=superposition(0,1);{quant uint8_t x=5;if(b)memset((void*)&x,value,1);value=99;if(x==7)phase(3.14159265358979323846);}inverse_fourier(b);return measure(b)!=1;}\n",
     NULL,
     1},
    {"lifetime-inactive-invalid-cpu",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){int *p=0;quant bool b=false;{quant uint8_t x=5;if(b)memset((void*)&x,*p,1);if(x==5)phase(3.14159265358979323846);}return measure(b)!=0;}\n",
     NULL,
     1},
    {"lifetime-borrowed-fill",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "static void fill(quant uint8_t *p,quant const bool *b){memset((void*)p,*b,1);}\n"
     "int main(void){quant bool b=superposition(0,1);{quant uint8_t x=5;fill(&x,&b);if(x==1)phase(3.14159265358979323846);}inverse_fourier(b);return measure(b)!=1;}\n",
     NULL,
     1},
    {"lifetime-signed-partial-word",
     "#include <quantum.h>\n"
     "#include <string.h>\n"
     "int main(void){quant bool b=superposition(0,1);{quant int16_t x=-1234;memset((char*)&x+1,b,1);if(x==302)phase(3.14159265358979323846);}inverse_fourier(b);return measure(b)!=1;}\n",
     NULL,
     1},
