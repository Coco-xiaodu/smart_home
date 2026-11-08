#ifndef PIR_H       
#define PIR_H

int pir_init(void); 
int pir_read(void);  /*读取pir状态*/
void pir_exit(void);  /*关闭pir*/
#endif