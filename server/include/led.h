#ifndef LED_H        /* 防止头文件被重复包含 */
#define LED_H

int led_init(void);  /* 初始化：打开 brightness 节点 */
void led_on(void);   /* 亮：写 1 */
void led_off(void);  /* 灭：写 0 */
void led_trigger(const char *string);  /*设置trigger*/

#endif
