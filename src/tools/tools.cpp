#include "tools.h"
#include <linux/input.h>
#include <fcntl.h>
#include <cstdio>
#include <linux/uinput.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <filesystem>
#include <vector>

Vector2::Vector2(int x, int y)
{
    this->x = (float)x;
    this->y = (float)y;
}
Vector2::Vector2(float x, float y)
{
    this->x = x;
    this->y = y;
}

Vector2::Vector2()
{
    x = 0;
    y = 0;
}

Vector2::Vector2(Vector2 &va)
{
    this->x = va.x;
    this->y = va.y;
}

Vector2 &Vector2::operator=(const Vector2 &other)
{
    // 防止自赋值
    if (this != &other)
    {
        this->x = other.x;
        this->y = other.y;
    }
    return *this;
}

Vector3::Vector3(int x, int y, int z)
{
    this->x = x;
    this->y = y;
    this->z = z;
}

Vector3::Vector3()
{
    x = 0;
    y = 0;
    z = 0;
}

void touch::InitTouchScreenInfo()
{
    for (const auto &entry : std::filesystem::directory_iterator("/dev/input/"))
    {
        int fd = open(entry.path().c_str(), O_RDWR);
        input_absinfo absinfo;
        ioctl(fd, EVIOCGABS(ABS_MT_SLOT), &absinfo);

        if (absinfo.maximum == 9)
        {
            this->touchScreenInfo.fd = open(entry.path().c_str(), O_RDWR);
            close(fd);
            break;
        }
    } // 遍历/dev/input/下所有eventX，如果ABS_MT_SLOT为9(即最大支持10点触控)就视为物理触摸屏
    input_absinfo absX, absY;
    ioctl(touchScreenInfo.fd, EVIOCGABS(ABS_MT_POSITION_X), &absX);
    ioctl(touchScreenInfo.fd, EVIOCGABS(ABS_MT_POSITION_Y), &absY);
    this->touchScreenInfo.width = absX.maximum;
    this->touchScreenInfo.height = absY.maximum;
}

void touch::InitScreenInfo()
{
    std::string window_size = exec("wm size");
    sscanf(window_size.c_str(), "Physical size: %dx%d", &this->screenInfo.width, &this->screenInfo.height);
} // 初始化屏幕分辨率,方向单独放在一个线程了

touch::touch()
{
    InitScreenInfo();
    InitTouchScreenInfo();
    for (int i = 0; i <10; i++) PhysicalSlot_2_FingerSlot[i]=-1;
    GetScreenorientationThread = std::thread(&touch::GetScrorientation, this);
    PTScreenEventToFingerThread = std::thread(&touch::PTScreenEventToFinger, this);
    this->uinputFd = open("/dev/uinput", O_RDWR);
    if (uinputFd < 0)
    {
        perror("打开uinput失败！！");
    }

    ioctl(uinputFd, UI_SET_PROPBIT, INPUT_PROP_DIRECT); // 设置为直接输入设备
    ioctl(uinputFd, UI_SET_EVBIT, EV_ABS);
    ioctl(uinputFd, UI_SET_EVBIT, EV_KEY);
    ioctl(uinputFd, UI_SET_EVBIT, EV_SYN); // 支持的事件类型

    ioctl(uinputFd, UI_SET_ABSBIT, ABS_MT_SLOT);
    ioctl(uinputFd, UI_SET_ABSBIT, ABS_MT_TOUCH_MAJOR);
    ioctl(uinputFd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    ioctl(uinputFd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
    ioctl(uinputFd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
    ioctl(uinputFd, UI_SET_ABSBIT, ABS_MT_PRESSURE);

    ioctl(uinputFd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(uinputFd, UI_SET_KEYBIT, BTN_TOOL_FINGER); // 支持的事件

    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_SPI;
    usetup.id.vendor = 0x6c90;
    usetup.id.product = 0x8fb0;
    strcpy(usetup.name, "Virtual Touch Screen for muchen"); // 驱动信息

    usetup.absmin[ABS_MT_POSITION_X] = 0;
    usetup.absmax[ABS_MT_POSITION_X] = touchScreenInfo.width;
    usetup.absfuzz[ABS_MT_POSITION_X] = 0;
    usetup.absflat[ABS_MT_POSITION_X] = 0;
    usetup.absmin[ABS_MT_POSITION_Y] = 0;
    usetup.absmax[ABS_MT_POSITION_Y] = touchScreenInfo.height;
    usetup.absfuzz[ABS_MT_POSITION_Y] = 0;
    usetup.absflat[ABS_MT_POSITION_Y] = 0;
    usetup.absmin[ABS_MT_PRESSURE] = 0;
    usetup.absmax[ABS_MT_PRESSURE] = 1000; // 触摸压力的最大最小值
    usetup.absfuzz[ABS_MT_PRESSURE] = 0;
    usetup.absflat[ABS_MT_PRESSURE] = 0;
    usetup.absmax[ABS_MT_SLOT] = 9;            // 同时支持最多9个触点
    usetup.absmax[ABS_MT_TOUCH_MAJOR] = 255;   // 与屏接触面的最大值
    usetup.absmax[ABS_MT_TRACKING_ID] = 65535; // 按键码ID累计叠加最大值

    write(uinputFd, &usetup, sizeof(usetup)); // 将信息写入即将创建的驱动

    ioctl(uinputFd, UI_DEV_CREATE); // 创建驱动

    ioctl(this->touchScreenInfo.fd, EVIOCGRAB, 0x1); // 独占输入,只有此进程才能接收到事件 --

    std::cout << "触摸屏宽高  " << touchScreenInfo.width << "   " << touchScreenInfo.height << std::endl;
    std::cout << "屏幕分辨率  " << screenInfo.width << "   " << screenInfo.height << std::endl;
    screenToTouchRatio = (float)(touchScreenInfo.width + touchScreenInfo.height) / (float)(screenInfo.width + screenInfo.height);
    if (screenToTouchRatio < 1 && screenToTouchRatio > 0.9)
    {
        screenToTouchRatio = 1;
    }
    sleep(2);
}

touch::~touch()
{
    ioctl(uinputFd, UI_DEV_DESTROY);
    close(uinputFd);
    PTScreenEventToFingerThread.detach();
    GetScreenorientationThread.detach();
}

void touch::PTScreenEventToFinger()
{
    input_event touchEvent{};
    std::vector<input_event> toectouchEventS; // 储存从两次syn事件中间的事件

    input_absinfo absinfo{};
    ioctl(touchScreenInfo.fd, EVIOCGABS(ABS_MT_SLOT), &absinfo); // 获取slot信息
    // printf("STARTED: ABS_MT_SLOT %d\n", absinfo.value);

    int Pyslot{absinfo.value};
    int NewSlot{-1};
    bool getNewSlot = false;
    while (true)
    {

        read(touchScreenInfo.fd, &touchEvent, sizeof(touchEvent));
        // printf("Read %d %d %d\n", touchEvent.type, touchEvent.code, touchEvent.value);
        if (touchEvent.type == EV_ABS)
        {
            if (touchEvent.code == ABS_MT_SLOT)
            {
                NewSlot = GetNewSlotByPy(touchEvent.value);
                // printf("Read SLOT %d -> Out SLOT %d\n", touchEvent.value,NewSlot);
                input_ABS_MT_SLOT(NewSlot);
                getNewSlot = true;
            }
            else if (touchEvent.code == ABS_MT_TRACKING_ID)
            {
                if (!getNewSlot)
                {
                    NewSlot=this->GetNoUseIndex();
                    // printf("Read SLOT %d -> Out SLOT %d\n", touchEvent.value,NewSlot);
                    input_ABS_MT_SLOT(NewSlot);
                    this->PhysicalSlot_2_FingerSlot[Pyslot] = NewSlot;
                    getNewSlot = true;
                }else{
                    input_ABS_MT_SLOT(NewSlot);
                }

                input_ABS_MT_TRACKING_ID(touchEvent.value != -1 ? this->GetTRACKING_ID(NewSlot) : -1);
            }
            else
            {
                FingersEvent.push_back(touchEvent);
            }
        }
        else if (touchEvent.type == EV_SYN)
        {
            FingersEvent.push_back(touchEvent);

            if (touchEvent.code == SYN_REPORT)
            {
                upLoadV2();
            }
        }else if (touchEvent.type == EV_KEY)
        {
            if (!(touchEvent.code == BTN_TOUCH || touchEvent.code == BTN_TOOL_FINGER))
            {
                FingersEvent.push_back(touchEvent);
            }
            
        }else{
            FingersEvent.push_back(touchEvent);
        }
        
    }
}

void touch::input_ABS_MT_TRACKING_ID(int id)
{
    FingersEvent.push_back(input_event{.type = EV_ABS, .code = ABS_MT_TRACKING_ID, .value = id});
}

void touch::input_ABS_MT_SLOT(int NewSlot)
{
    FingersEvent.push_back(input_event{.type = EV_ABS, .code = ABS_MT_SLOT, .value = NewSlot});
}

int touch::GetNewSlotByPy(int Pyslot)
{
    // printf("GetNewSlotByPy: Py:%d\n",Pyslot);
    int NewSlot{-1};
    int index = this->GetPyFinger(Pyslot);
    if (index != -1) // 这个屏幕slot已经分配过了，用之前分配的slot
    {
        NewSlot = index;
        // printf("index: %d\n",index);
    }
    else
    { // 没分配过，找个空闲的slot
        NewSlot = this->GetNoUseIndex();
        // printf("NewSlot: %d\n",NewSlot);

    }
    this->PhysicalSlot_2_FingerSlot[Pyslot] = NewSlot;
    // printf("GetNewSlotByPy: Py:%d New:%d\n",Pyslot,NewSlot);
    return NewSlot;
}

std::string touch::exec(std::string command)
{
    char buffer[128];
    std::string result = "";

    FILE *pipe = popen(command.c_str(), "r");
    while (!feof(pipe))
    {
        if (fgets(buffer, 128, pipe) != nullptr)
        {
            result += buffer;
        }
    }
    pclose(pipe);
    return result;
}

int touch::GetPyFinger(int slot)
{
    // printf("GetPyFinger:%d result:%d\n",slot,PhysicalSlot_2_FingerSlot[slot]);
    return PhysicalSlot_2_FingerSlot[slot];
}

void touch::GetScrorientation()
{
    while (true)
    {
        this->screenInfo.orientation = atoi(exec("dumpsys display | grep 'mCurrentOrientation' | cut -d'=' -f2").c_str());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

Vector2 touch::rotatePointx(Vector2 pos, const Vector2 wh)
{

    if (this->screenInfo.orientation == 0)
    {
        return pos;
    }
    Vector2 xy(pos.x, pos.y);
    if (this->screenInfo.orientation == 3)
    {
        xy.x = pos.y;
        xy.y =wh.y-pos.x;
    }
    else if (this->screenInfo.orientation == 1)
    {
        xy.x =  wh.x - pos.y ;
        xy.y =  pos.x;
    }
    return xy;
}

void touch::upLoadV2()
{
    if(isWriting)return;
    isWriting=true;
    for (input_event &event : FingersEvent)
    {
        bool flag=false;
        write(uinputFd, &event, sizeof(event));
        if (event.type == EV_ABS)
        {
            if (event.code == ABS_MT_SLOT)
            {
                currentSlot=event.value;
            }else if (event.code == ABS_MT_TRACKING_ID)
            {
                flag=true;
                if (event.value == -1 && this->IsLast())
                {
                    write(uinputFd,&event_BTN_TOUCH_UP , sizeof(event_BTN_TOUCH_UP));
                    write(uinputFd,&event_BTN_TOOL_FINGER_UP , sizeof(event_BTN_TOOL_FINGER_UP));
                }else if (this->IsFirst())
                {
                    write(uinputFd,&event_BTN_TOUCH_DOWN , sizeof(event_BTN_TOUCH_DOWN));
                    write(uinputFd,&event_BTN_TOOL_FINGER_DOWN , sizeof(event_BTN_TOOL_FINGER_DOWN));
                }
            }
            
            
        }else if (event.type == EV_SYN && event.code == SYN_REPORT && flag)
        {
            FreeSlot[currentSlot]=false;
            PhysicalSlot_2_FingerSlot[currentSlot]=-1;
            flag=false;
        }
        // else if (event.type == EV_ABS && event.code == ABS_MT_POSITION_X)
        // {
        //     printf("Write ABS_MT_POSITION_X: %d\n", event.value);
        // }
        // else if (event.type == EV_ABS && event.code == ABS_MT_POSITION_Y)
        // {
        //     printf("Write ABS_MT_POSITION_Y: %d\n", event.value);
        // }
        // else if (event.type == EV_SYN && event.code == SYN_REPORT)
        // {
        //     printf("Write SYN_REPORT: %d\n", event.value);
        // }else{
        //     printf("Write Other,type:%d code:%d value:%d\n",event.type,event.code, event.value);
        // }
    }
    FingersEvent.clear();
    for (input_event &event : VirtualFingersEvent)
    {
        bool flag=false;
        write(uinputFd, &event, sizeof(event));
        if (event.type == EV_ABS)
        {
            if (event.code == ABS_MT_SLOT)
            {
                currentSlot=event.value;
            }else if (event.code == ABS_MT_TRACKING_ID)
            {
                flag=true;
                if (event.value == -1 && this->IsLast())
                {
                    write(uinputFd,&event_BTN_TOUCH_UP , sizeof(event_BTN_TOUCH_UP));
                    write(uinputFd,&event_BTN_TOOL_FINGER_UP , sizeof(event_BTN_TOOL_FINGER_UP));
                }else if (this->IsFirst())
                {
                    write(uinputFd,&event_BTN_TOUCH_DOWN , sizeof(event_BTN_TOUCH_DOWN));
                    write(uinputFd,&event_BTN_TOOL_FINGER_DOWN , sizeof(event_BTN_TOOL_FINGER_DOWN));
                }
            }
            
            
        }else if (event.type == EV_SYN && event.code == SYN_REPORT && flag)
        {
            FreeSlot[currentSlot]=false;
            PhysicalSlot_2_FingerSlot[currentSlot]=-1;
            flag=false;
        }
        // else if (event.type == EV_ABS && event.code == ABS_MT_POSITION_X)
        // {
        //     printf("Write ABS_MT_POSITION_X: %d\n", event.value);
        // }
        // else if (event.type == EV_ABS && event.code == ABS_MT_POSITION_Y)
        // {
        //     printf("Write ABS_MT_POSITION_Y: %d\n", event.value);
        // }
        // else if (event.type == EV_SYN && event.code == SYN_REPORT)
        // {
        //     printf("Write SYN_REPORT: %d\n", event.value);
        // }else{
        //     printf("Write Other,type:%d code:%d value:%d\n",event.type,event.code, event.value);
        // }
    }
    VirtualFingersEvent.clear();
    isWriting=false;
}
bool touch::IsLast()
{
    int count{0};
    for (int i = 0; i < 10; i++)
    {
        if (FreeSlot[i]==true)
        {
            count++;
        }   
    }
    return count == 1;
}
int touch::GetNoUseIndex()
{
    for (int i = 0; i < 10; i++)
    {
        if (FreeSlot[i]==false)
        {
            FreeSlot[i] = true;
            return i;
        }
    }
    
    return -1;
}

int touch::GetTRACKING_ID(int slot)
{
    // currentTrakingID =  (currentTrakingID + 1) % 10;
    return FingerSlot_2_TRACKING_ID[slot];
}
std::vector<input_event> touch::createNewFinger(int x, int y, int &retSlot)
{
    input_absinfo absinfo{};
    retSlot = this->GetNoUseIndex();
    int tracking_id = this->GetTRACKING_ID(retSlot);

    std::vector<input_event> firstDown_event = {
        {.type = EV_ABS, .code = ABS_MT_SLOT, .value = retSlot},
        {.type = EV_ABS, .code = ABS_MT_TRACKING_ID, .value = tracking_id},
        {.type = EV_ABS, .code = ABS_MT_WIDTH_MAJOR, .value = 0x0000000b},
        {.type = EV_ABS, .code = ABS_MT_TOUCH_MAJOR, .value = 0x0000000d},
        {.type = EV_ABS, .code = ABS_MT_TOUCH_MINOR, .value = 0x0000000d},
        {.type = EV_ABS, .code = ABS_MT_PRESSURE, .value = 0x0000000d},
        {.type = EV_ABS, .code = ABS_MT_POSITION_X, .value = x},
        {.type = EV_ABS, .code = ABS_MT_POSITION_Y, .value = y},
        {.type = EV_SYN, .code = SYN_REPORT, .value = 0},
        {.type = EV_ABS, .code = ABS_MT_SLOT, .value = currentSlot},
    };
    return firstDown_event;
}
int touch::touch_down(Vector2 pos)
{
    Vector2 NewPos = this->rotatePointx(pos, {this->screenInfo.width, this->screenInfo.height});
    NewPos.x *= screenToTouchRatio;
    NewPos.y *= screenToTouchRatio;
    printf("touch_down %f %f\n", NewPos.x, NewPos.y);
    int retSlot{0};
    std::vector<input_event> event = this->createNewFinger(NewPos.x, NewPos.y, retSlot);
    VirtualFingersEvent.insert(VirtualFingersEvent.end(), event.begin(), event.end());
    this->upLoadV2();
    return retSlot;
}

void touch::touch_move(const int &slotId, Vector2 pos)
{
    Vector2 NewPos = this->rotatePointx(pos, {this->screenInfo.width, this->screenInfo.height});
    NewPos.x *= screenToTouchRatio;
    NewPos.y *= screenToTouchRatio;
    struct input_event move_event[] = {
        {.type = EV_ABS, .code = ABS_MT_SLOT, .value = slotId},
        {.type = EV_ABS, .code = ABS_MT_POSITION_X, .value = (int)NewPos.x},
        {.type = EV_ABS, .code = ABS_MT_POSITION_Y, .value = (int)NewPos.y},
        {.type = EV_SYN, .code = SYN_REPORT, .value = 0},
        {.type = EV_ABS, .code = ABS_MT_SLOT, .value = currentSlot}};
    int arrCount = sizeof(move_event) / sizeof(move_event[0]);
    VirtualFingersEvent.insert(VirtualFingersEvent.end(), move_event, move_event + arrCount);

    this->upLoadV2();
}

void touch::touch_up(const int &slotId)
{
    std::vector<input_event> up_event = {
        {.type = EV_ABS, .code = ABS_MT_SLOT, .value = slotId},
        {.type = EV_ABS, .code = ABS_MT_TRACKING_ID, .value = -1},
        {.type = EV_SYN, .code = SYN_REPORT, .value = 0},
        {.type = EV_ABS, .code = ABS_MT_SLOT, .value = currentSlot}};
    VirtualFingersEvent.insert(VirtualFingersEvent.end(), up_event.begin(), up_event.end());
    this->upLoadV2();
}

bool touch::IsFirst()
{
    int count{0};
    for (int i = 0; i < 10; i++)
    {
        if (FreeSlot[i])
        {
            count++;
        }
    }
    
    return count==1;
}