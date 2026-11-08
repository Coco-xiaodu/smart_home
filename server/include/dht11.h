#ifndef DHT11_H
#define DHT11_H

int dht11_init(void);  /* 初始化 DHT11，返回 0 成功，-1 失败 */
int dht11_read(void);  /* 读取温湿度，返回 0 成功，-1 失败 */

#endif

