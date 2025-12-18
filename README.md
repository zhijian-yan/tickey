# tickey
## 跨平台的按键检测库，基于tick实现，简单易用
# 特性
- 支持检测短按、长按、双击、连击等状态
- 底层由tick驱动，tick时长可自由设置
- 采用面向对象的思想进行封装
- 硬件无关，跨平台，无第三方依赖
- 资源延迟删除的安全设计

# 移植
- **tickey**的代码为C语言设计，支持C++环境，移植无需其他步骤，直接将源代码添加到项目中即可
- 按键实例的创建默认使用C语言标准库的`malloc`函数，如有特殊的内存分配需求，可以将头文件中`tkey_malloc`和`tkey_free`宏替换为项目中所使用的内存分配函数

# 注意事项
- 不能在中断中调用`tkey_delete`函数，会有`Use-After-Free`风险
- `tkey_delete`仅释放内存，不会设置指针为NULL，请手动设置
- 可以在event中调用`tkey_delete`函数，它会延迟删除

# 使用
## 第一步 创建按键实例
创建一个按键实例并初始化，可以选择创建默认按键实例或者自定义按键实例
### 创建默认按键实例
调用`tkey_create_default`函数创建默认按键实例
```c
tkey_handle_t key = tkey_create_default(tkey_event_cb, tkey_detect_cb, user_data);
```
默认按键实例的属性如下:
- 按键扫描处理函数的工作频率:`50Hz`
- 去抖时间:`1 tick` (20ms@50Hz)
- 长按时间:`25 tick` (500ms@50Hz)
- 连续按下时间间隔:`15 tick` (300ms@50Hz)
### 创建自定义按键实例
调用`tkey_create`函数创建自定义按键实例，使用`tkey_config_t`结构体初始化按键实例
```c
tkey_config_t tkey_config = 
{
    event_cb = tkey_event_cb,
    detect_cb = tkey_detect_cb,
    user_data = user_data,
    hold_ticks = 25,
    debounce_ticks = 1,
    multi_press_interval_ticks = 15,
};

tkey_handle_t key = tkey_create(&tkey_config);
```
结构体参数如下:
- `event_cb`:事件回调函数
- `detect_cb`:检测回调函数
- `user_data`:传入回调函数的用户数据
- `hold_ticks`:长按持续时间
- `debounce_ticks`:去抖时间
- `multi_press_interval_ticks`:连续按下间隔时间
### 创建按键实例句柄数组
使用`TKEY_HANDLE_ARRAY_DEFINE(name, num)`宏定义零初始化的按键实例句柄的数组，`name`是数组名，`num`是数组中按键实例句柄的个数
```c
TKEY_HANDLE_ARRAY_DEFINE(key, 3); // 定义数组key，包含3个按键实例句柄
```
按键实例句柄用来接收`tkey_create_default`函数或者`tkey_create`函数返回的按键实例，数组可以传入`tkey_mutli_handler`处理程序来对多个按键实例进行检测
## 第二步 编写回调函数
### 检测回调函数
```c
int tkey_detect_cb(void *user_data)
{
    int pin = (int)user_data;
    if(gpio_read(pin) == 0) // 0:按下时电平为低电平 1:按下时电平为高电平
        return 1; // 检测到按下返回1
    else
        return 0; // 检测到释放返回0
}
```
可以在注册回调函数时通过`user_data`传入需要检测的引脚，这样可以将这个检测回调函数注册在不同的按键实例中
### 事件回调函数
所有的按键事件如下:
- `TKEY_EVENT_PRESS`:按键按下时的事件
- `TKEY_EVENT_RELEASE`:按键释放时的事件
- `TKEY_EVENT_LONG_PRESS`:判定为长按时的事件
- `TKEY_EVENT_LONG_RELEASE`:按键长按后释放时的事件
- `TKEY_EVENT_MULTI_PRESS`:判定为多次按下时的事件
- `TKEY_EVENT_MULTI_RELEASE`:按键多次按下后释放时的事件
- `TKEY_EVENT_PRESS_TIMEOUT`:按键在按下状态下多次按下检测超时的事件
- `TKEY_EVENT_RELEASE_TIMEOUT`:按键在释放状态下多次按下检测超时的事件
- `TKEY_EVENT_ALL_PRESS`:所有按下的事件，包括按下、长按和多次按下
- `TKEY_EVENT_ALL_RELEASE`:所有释放的事件，包括释放、长按后释放、多次按下后释放
- `TKEY_EVENT_DEFAULT_PRESS`:默认的按键按下事件，包括按下和多次按下
- `TKEY_EVENT_DEFAULT_RELEASE`:默认的按键释放事件，包括释放、长按后释放、多次按下后释放

不同的事件可以通过`|`与操作来完成多状态检测
```c
void tkey_event_cb(tkey_handle_t key, tkey_event_t event, uint8_t multi_press_count, void *user_data);
```
事件回调函数的参数如下:
- `key`:按键实例句柄
- `event`:事件
- `multi_press_count`:多次按下的次数
- `user_data`:用户数据
#### 例子1：默认按键事件检测
```c
void tkey_event_cb(tkey_handle_t key, tkey_event_t event, uint8_t multi_press_count, void *user_data)
{
    if(event & TKEY_EVENT_DEFAULT_PRESS)
        printf("key pressed %d times\r\n", multi_press_count);
    else if(event & TKEY_EVENT_DEFAULT_RELEASE)
        printf("key released\r\n");
}
```
默认的按下事件包括按下事件和多次按下事件，不包括长按事件，如果包括长按会导致在长按时先触发按下事件再触发长按事件，从而导致一次长按却触发两次回调函数

默认释放事件包括释放事件、长按后释放事件、多次按下后释放事件，只在按键释放时触发回调函数
#### 例子2：多个按键实例共用一个事件回调函数
```c
void tkey_event_cb(tkey_handle_t key, tkey_event_t event, uint8_t multi_press_count, void *user_data)
{
    switch(key)
    {
        case key1:
        if(event & TKEY_EVENT_DEFAULT_PRESS)
            printf("key1 pressed %d times\r\n", multi_press_count);
        break;
        case key2:
        if(event & TKEY_EVENT_DEFAULT_PRESS)
            printf("key2 pressed %d times\r\n", multi_press_count);
        break;
    }
}
```
多个按键可以共用一个事件回调函数，他们之间通过`tkey_handle_t key`参数进行区分
#### 例子3：按下和长按共存
```c
void tkey_event_cb(tkey_handle_t key, tkey_event_t event, uint8_t multi_press_count, void *user_data)
{
    if(event & TKEY_EVENT_RELEASE) // 按下事件
        printf("key pressed once\r\n");
    else if(event & TKEY_EVENT_LONG_PRESS) // 长按事件
        printf("key long pressed\r\n");
}
```
一般情况下，在长按事件触发前会先触发按下事件

长按事件的触发时刻是在长按时间达到阈值的那一刻，为了避免在长按事件触发前先触发按下事件，应当以释放按键为结束标志
#### 例子4：按下和多次按下共存
```c
void tkey_event_cb(tkey_handle_t key, tkey_event_t event, uint8_t multi_press_count, void *user_data)
{
    if(event & TKEY_EVENT_RELEASE_TIMEOUT)
    {
        switch(multi_press_count)
        {
            case 1:printf("single click\r\n"); // 单击
            break;
            case 2:printf("double click\r\n"); // 双击
            break;
            case 3:printf("triple click\r\n"); // 三击
            break;
        }
    }
}
```
一般情况下，在多次按下的次数大于一次时会先触发一次按下事件

为避免这种情况发生，通过`多次按下检测`结束后的`超时事件`延迟判定是按下还是多次按下，超时事件的发生表明结束了本次连续按下的判定，连续按下的次数将重新计数
- `TKEY_EVENT_PRESS_TIMEOUT`:按键按下后到下一次按下的时间超过`多次按键检测`的阈值而产生的超时事件，超时事件触发时按键为按下状态
- `TKEY_EVENT_RELEASE_TIMEOUT`:按键按下后到下一次按下的时间超过`多次按键检测`的阈值而产生的超时事件，超时事件触发时按键为释放状态

延迟判定会导致事件回调函数延迟`multi_press_interval_ticks`时间后执行
#### 例子5：按下、多次按下、长按共存
```c
void tkey_event_cb(tkey_handle_t key, tkey_event_t event, uint8_t multi_press_count, void *user_data)
{
    if(event & TKEY_EVENT_LONG_PRESS) // 长按事件
        printf("long pressed\r\n");
    else if(event & TKEY_EVENT_RELEASE_TIMEOUT)
    {
        switch(multi_press_count)
        {
            case 1:printf("single click\r\n"); // 单击
            break;
            case 2:printf("double click\r\n"); // 双击
            break;
            case 3:printf("triple click\r\n"); // 三击
            break;
        }
    }
}
```
`例子3`中通过释放按键来区分按下事件和长按事件，为了兼容多次按下事件，在多次按下事件中以释放按键为结束标志

在上面的代码中，连续按下按键后保持按下状态到长按事件触发时不会触发多次按下事件

如果需要触发多次按下事件，则需要添加对`TKEY_EVENT_PRESS_TIMEOUT`事件的处理
```c
void tkey_event_cb(tkey_handle_t key, tkey_event_t event, uint8_t multi_press_count, void *user_data)
{
    if(event & TKEY_EVENT_LONG_PRESS) // 长按事件
        printf("long pressed\r\n");
    else if(event & TKEY_EVENT_RELEASE_TIMEOUT)
    {
        switch(multi_press_count)
        {
            case 1:printf("single click triggered by release timeout\r\n"); // 单击
            break;
            case 2:printf("double click triggered by release timeout\r\n"); // 双击
            break;
            case 3:printf("triple click triggered by release timeout\r\n"); // 三击
            break;
        }
    }
    else if(event & TKEY_EVENT_PRESS_TIMEOUT)
    {
        switch(multi_press_count)
        {
            case 2:printf("double click triggered by press timeout\r\n"); // 双击
            break;
            case 3:printf("triple click triggered by press timeout\r\n"); // 三击
            break;
        }
    }
}
```
## 第三步 周期性调用处理程序
初始化完成后需要周期性地调用`tkey_handler`函数处理按键的扫描事件，`tkey_handler`函数只能处理一个按键实例
```c
void hardtimer_callback(void) // 在定时器中断中周期性调用，定时周期20ms@50Hz
{
    tkey_handler(&key);
}
```
或者简单地使用循环周期性地调用
```c
while(1)
{
    tkey_handler(&key);
    delay_ms(20);
}
```
`tkey_mutli_handler`函数可以处理多个`相同配置`的按键实例，多个按键实例通过按键实例句柄的数组传入`tkey_mutli_handler`函数
```c
TKEY_HANDLE_ARRAY_DEFINE(key_array, 3);
key_array[0] = tkey_create_default(config); // 相同配置
key_array[1] = tkey_create_default(config);
key_array[2] = tkey_create_default(config);
```
使用`TKEY_HANDLE_ARRAY_GET_NUM(name)`获取按键数组的大小，同时需要注意`tkey_mutli_handler`函数的第一个参数传入数组名即可，无需`&`符号
```c
while(1)
{
    tkey_mutli_handler(key_array, TKEY_HANDLE_ARRAY_GET_NUM(key_array));
    delay_ms(20);
}
```
如果需要不同配置的按键，需要为不同配置的按键调用`tkey_handler`函数处理
```c
TKEY_HANDLE_ARRAY_DEFINE(key_array, 3);
key_array[0] = tkey_create_default(config_1); // 不同配置
key_array[1] = tkey_create_default(config_2);
key_array[2] = tkey_create_default(config_2);

void timer_interrupt1(void)
{
    tkey_handler(&key_array[0]);
}

void timer_interrupt2(void)
{
    tkey_handler(&key_array[1]);
    tkey_handler(&key_array[2]);
}
```
## 第四步 开始享受tickey吧！
